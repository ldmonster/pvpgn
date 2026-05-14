// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/tcp_bridge.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include "application/ports/connection_handler.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "infra/net/io_runtime.hpp"
#include "infra/net/tcp_acceptor.hpp"
#include "infra/net/tcp_session.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "integration/legacy_bnetd/legacy_bnet_frame_router.hpp"

// Reach into the legacy server for the reserved TCP listener list,
// the v3 owned-socket factory, and the per-connection v3 router
// ownership APIs (38c/38e).
#include "common/setup_before.h"
#include "bnetd/server.h"
#include "bnetd/prefs.h"
#include "bnetd/connection.h"
#include "common/setup_after.h"

namespace pvpgn::integration::legacy_bnetd {

namespace {

namespace asio = boost::asio;

// Build a network-order `sockaddr_in` from an Asio v4 endpoint so
// that the legacy callback can read peer addr/port consistently
// with what it would see from `psock_accept()`.
struct sockaddr_in to_sockaddr_in(const asio::ip::tcp::endpoint& ep)
{
    struct sockaddr_in out{};
    out.sin_family = AF_INET;
    out.sin_port   = htons(ep.port());
    if (ep.address().is_v4()) {
        const auto bytes = ep.address().to_v4().to_bytes();
        std::memcpy(&out.sin_addr, bytes.data(), bytes.size());
    }
    return out;
}

}  // namespace

class TcpBridgeImpl {
public:
    TcpBridgeImpl() = default;
    explicit TcpBridgeImpl(infra::net::IoRuntime& runtime) noexcept
        : external_runtime_(&runtime) {}
    ~TcpBridgeImpl() { stop(); }

private:
    // ----------------------------------------------------------------
    // 38f: v3-owned bnet session bundle.
    //
    // When `v3_tcp_session_mode != 0`, each accepted bnet socket is
    // wrapped in this trio: an Asio-driven `TcpSession`, a
    // `TcpSessionEgress` adapter that turns the application-port
    // `IConnectionEgress` interface into `session->send`/`close`,
    // and a `LegacyBnetFrameRouter` that frames inbound bytes and
    // delivers them to `handle_init_packet` / `handle_bnet_packet`
    // via the linked-variant dispatch hook. `legacy_conn` is the
    // `t_connection*` allocated by `server_handle_v3_owned_bnet_socket`;
    // it is marked `v3_owns_socket = 1` so legacy `conn_destroy`
    // will NOT psock_close the fd (Asio owns it).
    //
    // Bundles are stored in `sessions_` keyed by `shared_ptr`.
    // The lifetime is: shared_ptr held by `sessions_` + shared_ptr
    // captured by the session callbacks. When the session closes,
    // the teardown lambda removes the bundle from `sessions_`,
    // dropping the last `shared_ptr` once the on_close callback
    // returns.
    class TcpSessionEgress : public application::ports::IConnectionEgress {
    public:
        explicit TcpSessionEgress(
            std::shared_ptr<infra::net::TcpSession> session) noexcept
            : session_(std::move(session)) {}

        void send(std::vector<std::byte> bytes) override {
            if (auto s = session_.lock()) s->send(std::move(bytes));
        }
        void close() override {
            if (auto s = session_.lock()) s->close();
        }

    private:
        std::weak_ptr<infra::net::TcpSession> session_;
    };

    struct V3OwnedSession {
        std::shared_ptr<infra::net::TcpSession> session;
        std::unique_ptr<TcpSessionEgress>       egress;
        std::unique_ptr<LegacyBnetFrameRouter>  router;
        ::pvpgn::bnetd::t_connection*           legacy_conn = nullptr;
        std::atomic<bool>                       torn_down{false};
    };

    // Invoked from `TcpSession::on_close` on an Asio worker thread.
    // Idempotent: a second call (e.g. from manual close + peer
    // close) is a no-op. The v3-side bookkeeping (remove from
    // `sessions_`) runs synchronously on whatever thread called
    // teardown; the legacy-touching work (`conn_set_v3_router`,
    // `conn_destroy`) is posted to the legacy main loop so it
    // shares the single-threaded invariants of
    // `timerlist_check_timers` / `connlist_reap`. The bundle's
    // lifetime is extended via the lambda's `shared_ptr<owned>`
    // capture until the main-loop callback runs and releases it.
    void teardown_session(std::shared_ptr<V3OwnedSession> owned) noexcept {
        if (!owned) return;
        if (owned->torn_down.exchange(true)) return;

        // Step A: drop our strong ref from `sessions_`. The lambda
        // below holds another strong ref so `owned` survives until
        // the main-loop callback runs.
        {
            std::lock_guard<std::mutex> g{sessions_mu_};
            sessions_.erase(
                std::remove_if(sessions_.begin(), sessions_.end(),
                               [&owned](const std::shared_ptr<V3OwnedSession>& s) {
                                   return s.get() == owned.get();
                               }),
                sessions_.end());
        }

        // Step B: destroy the legacy connection on the legacy
        // main-loop thread. If the main loop has already exited,
        // `server_post_to_main` drops the callback -- the legacy
        // `_shutdown_conns` will reap the connection (skipping
        // the fd close because `v3_owns_socket = 1`), and the
        // bundle is freed at process exit when `pending_main_q`
        // is cleared.
        ::pvpgn::bnetd::server_post_to_main([owned]() mutable {
            auto* c = owned->legacy_conn;
            if (c != nullptr) {
                // Clear the v3-router pointer first so any racing
                // `conn_push_outqueue` sees a NULL router and
                // routes through legacy (which is a no-op because
                // the outqueue is drained below).
                ::pvpgn::bnetd::conn_set_v3_router(c, nullptr);
                // Destroy. `v3_owns_socket=1` skips fd close;
                // `DESTROY_FROM_DEADLIST` makes `conn_destroy`
                // look up the connlist element itself.
                ::pvpgn::bnetd::conn_destroy(
                    c, nullptr, DESTROY_FROM_DEADLIST);
                owned->legacy_conn = nullptr;
            }
            // `owned` drops here -> V3OwnedSession destructor ->
            // TcpSession destructor -> fd close (Asio side).
        });
    }

    // Build a `V3OwnedSession` from an accepted Asio socket. Called
    // from the raw_handler when `v3_tcp_session_mode != 0`. Returns
    // false (and closes the socket) on any failure; on success the
    // bundle is stored in `sessions_` and the TcpSession is started.
    bool spawn_v3_owned_session(std::size_t listener_index,
                                asio::ip::tcp::socket sock,
                                const sockaddr_in& peer) {
        // Build the session first so we know the fd; Asio's
        // `native_handle()` is non-const so we need the session
        // built before we can read it.
        auto session = infra::net::TcpSession::create(std::move(sock));
        const int fd = session->native_handle_int();
        if (fd < 0) {
            bridge_log(core::LogLevel::Warn, "v3_tcp_bridge",
                       "v3-owned session: native_handle returned -1 "
                       "for freshly accepted socket; dropping");
            return false;
        }

        // Hand the fd to legacy via the factory. The factory marks
        // the new t_connection as `v3_owns_socket = 1` BEFORE any
        // teardown path can fire, so a failure between this point
        // and `sessions_` insertion is still safe -- conn_destroy
        // will not psock_close our fd.
        auto* legacy_conn =
            ::pvpgn::bnetd::server_handle_v3_owned_bnet_socket(
                listener_index, fd, &peer);
        if (legacy_conn == nullptr) {
            // Factory already logged. Session destructor will close
            // the fd.
            return false;
        }

        auto owned = std::make_shared<V3OwnedSession>();
        owned->session     = std::move(session);
        owned->egress      = std::make_unique<TcpSessionEgress>(owned->session);
        owned->router      = std::make_unique<LegacyBnetFrameRouter>(
            reinterpret_cast<LegacyBnetConnection*>(legacy_conn),
            ConnectionClass::Init);
        owned->legacy_conn = legacy_conn;

        // Wire conn -> router so `conn_push_outqueue` redirects to
        // the v3 egress (38c slot).
        ::pvpgn::bnetd::conn_set_v3_router(
            legacy_conn, owned->router.get());

        // Start the router so it has its egress channel before the
        // first byte arrives.
        owned->router->start(*owned->egress);

        // Capture a weak shared_ptr in the session callbacks so the
        // callbacks do not extend the bundle's lifetime by
        // themselves -- `sessions_` is the single owner.
        std::weak_ptr<V3OwnedSession> weak = owned;
        owned->session->set_on_bytes(
            [weak](core::ByteView bytes) {
                if (auto s = weak.lock()) {
                    s->router->on_bytes(bytes);
                }
            });
        owned->session->set_on_close(
            [this, weak](const boost::system::error_code&) {
                if (auto s = weak.lock()) {
                    teardown_session(s);
                }
            });

        {
            std::lock_guard<std::mutex> g{sessions_mu_};
            sessions_.push_back(owned);
        }

        owned->session->start();
        return true;
    }

public:
    core::Result<std::size_t> start()
    {
        if (running_.exchange(true)) {
            return core::fail(core::Error{
                core::StatusCode::FailedPrecondition,
                "TcpBridge already installed"});
        }

        const auto listeners = pvpgn::bnetd::server_get_bnet_tcp_listeners();
        if (listeners.empty()) {
            running_ = false;
            return core::fail(core::Error{
                core::StatusCode::FailedPrecondition,
                "no reserved bnet TCP listeners; did you forget "
                "server_set_skip_legacy_tcp_fdwatch(true)?"});
        }

        // Share an externally provided runtime when available,
        // otherwise fall back to owning our own.
        infra::net::IoRuntime* runtime_ref = external_runtime_;
        if (runtime_ref == nullptr) {
            runtime_ = std::make_unique<infra::net::IoRuntime>();
            runtime_ref = runtime_.get();
        }
        acceptors_.reserve(listeners.size());

        std::size_t installed = 0;
        for (std::size_t i = 0; i < listeners.size(); ++i) {
            const auto& info = listeners[i];

            auto acc = std::make_unique<infra::net::TcpAcceptor>(
                *runtime_ref,
                /*factory=*/infra::net::TcpAcceptor::SessionFactory{});

            // Install raw handler before adopting so the first
            // accept (driven by `do_accept()` inside adopt) goes
            // through us, not the unused SessionFactory.
            acc->set_raw_handler([this, listener_index = i](asio::ip::tcp::socket sock) {
                boost::system::error_code ec;
                auto peer = sock.remote_endpoint(ec);
                if (ec) {
                    // No peer addr -> safer to drop than to fabricate one.
                    sock.close(ec);
                    return;
                }
                auto caddr = to_sockaddr_in(peer);

                // 38f: live flip. When `v3_tcp_session_mode != 0`,
                // hand the accepted socket to a v3 `TcpSession` +
                // `LegacyBnetFrameRouter` pair and KEEP the fd on
                // the Asio side. Legacy fdwatch never sees it.
                // The `t_connection*` is still allocated via the
                // legacy factory so all the per-conn state (ip-ban
                // check, keepalive, conn list, initkill timer)
                // remains consistent with the fdwatch path.
                //
                // Smoke-test contract: this code path has been
                // structurally validated (build-clean across all
                // three matrices) but has NOT been driven by a real
                // BNet/D2DV client from the agent side. Operators
                // turning on `v3_tcp_session_mode = 1` should
                // expect to encounter at least one runtime issue
                // worth fixing in a follow-up batch (38g).
                if (pvpgn::bnetd::prefs_get_v3_tcp_session_mode() != 0) {
                    if (spawn_v3_owned_session(
                            listener_index, std::move(sock), caddr)) {
                        return;
                    }
                    // spawn failed; sock has already been closed
                    // by the session destructor or the factory.
                    return;
                }

                // Legacy path (default): release the native fd
                // from Asio so legacy owns it, then hand it to
                // the existing accept-completion callback.
                const auto native = sock.release(ec);
                if (ec) {
                    sock.close(ec);
                    return;
                }
                pvpgn::bnetd::server_handle_v3_accepted_bnet_socket(
                    listener_index,
                    static_cast<int>(native),
                    &caddr);
            });

            auto adopted = acc->adopt_native_handle(info.ssocket, /*ipv6=*/false);
            if (!adopted.has_value()) {
                continue;
            }
            acceptors_.push_back(std::move(acc));
            acceptor_listener_indices_.push_back(i);
            ++installed;
        }

        if (installed == 0) {
            runtime_.reset();
            running_ = false;
            return core::fail(core::Error{
                core::StatusCode::Internal,
                "TcpBridge: no TCP listeners could be adopted"});
        }

        // `IoRuntime::run()` is idempotent: if the runtime is shared
        // with another bridge that has already started it, this is a
        // no-op.
        runtime_ref->run(1);
        return installed;
    }

    void stop()
    {
        if (!running_.exchange(false)) return;
        // Release each listening fd from legacy's `_shutdown_addrs`
        // path and close it via Asio.  This avoids the previous
        // "keep acceptors alive until process exit so legacy's
        // psock_close lands on an already-invalid fd" workaround.
        //
        // Order matters: tell legacy we own the fd *before* we ask
        // Asio to close it.  If we closed first, legacy's
        // `psock_close()` would race fd-number reuse between our
        // close and the kernel handing the same number to another
        // socket.
        for (std::size_t k = 0; k < acceptors_.size(); ++k) {
            const std::size_t lidx = acceptor_listener_indices_[k];
            (void)pvpgn::bnetd::server_release_bnet_tcp_listener_fd(lidx);
            // `TcpAcceptor::close()` is idempotent and already
            // swallows any boost::system::error_code returned by the
            // underlying Asio close, so it is safe to call here
            // even when the acceptor was never fully started or has
            // already been torn down.
            acceptors_[k]->close();
        }
        acceptors_.clear();
        acceptor_listener_indices_.clear();

        // 38g (shutdown ordering): drain v3-owned sessions before
        // stopping the runtime. We close each session under no
        // mutex so the on_close callback can re-enter
        // `teardown_session` (which acquires `sessions_mu_`)
        // without deadlock. The post to legacy main loop is
        // dropped if the main loop has already exited
        // (`_shutdown_conns` will then reap the connection itself).
        std::vector<std::shared_ptr<V3OwnedSession>> to_close;
        {
            std::lock_guard<std::mutex> g{sessions_mu_};
            to_close = sessions_;  // copy strong refs
        }
        for (auto& s : to_close) {
            if (s && s->session) s->session->close();
        }
        to_close.clear();

        // Only stop a runtime we own; shared runtimes are stopped
        // by their owner.
        if (runtime_) runtime_->stop();
    }

    std::size_t acceptor_count() const noexcept { return acceptors_.size(); }

private:
    std::atomic<bool>                                          running_{false};
    infra::net::IoRuntime*                                     external_runtime_{nullptr};
    std::unique_ptr<infra::net::IoRuntime>                     runtime_;
    std::vector<std::unique_ptr<infra::net::TcpAcceptor>>      acceptors_;
    std::vector<std::size_t>                                   acceptor_listener_indices_;

    // 38f: registry of v3-owned bnet sessions. Each entry is the
    // sole strong reference to a `V3OwnedSession` bundle; the
    // bundle is freed when `teardown_session` removes the entry.
    std::mutex                                                 sessions_mu_;
    std::vector<std::shared_ptr<V3OwnedSession>>               sessions_;
};

TcpBridge::TcpBridge() : impl_(std::make_unique<TcpBridgeImpl>()) {}
TcpBridge::TcpBridge(infra::net::IoRuntime& runtime)
    : impl_(std::make_unique<TcpBridgeImpl>(runtime)) {}
TcpBridge::~TcpBridge() = default;

core::Result<std::size_t> TcpBridge::install()  { return impl_->start(); }
void                       TcpBridge::shutdown() { impl_->stop(); }
std::size_t                TcpBridge::acceptor_count() const noexcept {
    return impl_->acceptor_count();
}

}  // namespace pvpgn::integration::legacy_bnetd
