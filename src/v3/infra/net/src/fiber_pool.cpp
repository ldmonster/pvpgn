// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/net/fiber_pool.hpp"

#if defined(PVPGN_V3_HAVE_FIBER)

#include <memory>
#include <utility>

#include <boost/asio/post.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/fiber/operations.hpp>
#include <boost/system/error_code.hpp>

#if defined(_WIN32)
#  include <winsock2.h>
#else
#  include <unistd.h>
#endif

#include "core/error.hpp"
#include "infra/net/asio_round_robin.hpp"
#include "infra/net/tcp_session.hpp"

namespace pvpgn::infra::net {

namespace {
core::Error make_error(core::StatusCode code,
                       const boost::system::error_code& ec) {
    return core::Error{code, ec.message()};
}

// Close a raw OS socket handle in a portable manner. Used only
// on the failure path of `assign()`; the happy path keeps the
// fd wrapped in an asio socket.
inline void close_native_socket(
    boost::asio::ip::tcp::socket::native_handle_type fd) noexcept {
#if defined(_WIN32)
    ::closesocket(fd);
#else
    ::close(fd);
#endif
}
}  // namespace

FiberPool::FiberPool() = default;

FiberPool::~FiberPool() {
    stop();
}

void FiberPool::start(std::size_t threads) {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return;
    if (threads == 0) threads = 1;

    workers_.reserve(threads);
    for (std::size_t i = 0; i < threads; ++i) {
        workers_.push_back(std::make_unique<Worker>());
    }

    // Spin up each worker thread with its own round_robin scheduler.
    for (auto& w : workers_) {
        Worker* raw = w.get();
        raw->thread = std::thread([raw] {
            // Aliasing shared_ptr — round_robin needs a
            // shared_ptr<io_context> but we don't actually share
            // ownership; the Worker outlives the thread by RAII.
            std::shared_ptr<boost::asio::io_context> ctx_alias(
                std::shared_ptr<void>{}, &raw->ctx);
            boost::fibers::use_scheduling_algorithm<
                boost::fibers::asio::round_robin>(ctx_alias);
            raw->ctx.run();
        });
    }
}

void FiberPool::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) return;

    close_acceptor();

    // Drop work guards so each ctx can drain.
    for (auto& w : workers_) {
        w->work_guard.reset();
        w->ctx.stop();
    }
    for (auto& w : workers_) {
        if (w->thread.joinable()) w->thread.join();
    }
    workers_.clear();
    next_worker_.store(0);
}

boost::asio::any_io_executor FiberPool::next_executor() noexcept {
    return pick_worker_round_robin().ctx.get_executor();
}

FiberPool::Worker& FiberPool::pick_worker_round_robin() noexcept {
    const auto n = workers_.size();
    const auto i = next_worker_.fetch_add(1, std::memory_order_relaxed) % n;
    return *workers_[i];
}

core::Result<boost::asio::ip::tcp::endpoint, core::Error>
FiberPool::accept(const std::string& host,
                  std::uint16_t      port,
                  SessionHandler     handler,
                  std::size_t        inbox_capacity) {
    if (workers_.empty()) {
        return core::fail(core::Error{
            core::StatusCode::FailedPrecondition,
            "FiberPool::accept called before start()"});
    }

    boost::system::error_code ec;
    const auto address = boost::asio::ip::make_address(host, ec);
    if (ec) {
        return core::fail(
            make_error(core::StatusCode::InvalidArgument, ec));
    }

    auto& w0 = *workers_[0];
    auto acc = std::make_unique<boost::asio::ip::tcp::acceptor>(w0.ctx);

    using boost::asio::ip::tcp;
    acc->open(address.is_v4() ? tcp::v4() : tcp::v6(), ec);
    if (ec) {
        return core::fail(
            make_error(core::StatusCode::Internal, ec));
    }
    acc->set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec) {
        return core::fail(
            make_error(core::StatusCode::Internal, ec));
    }
    acc->bind(tcp::endpoint(address, port), ec);
    if (ec) {
        return core::fail(
            make_error(core::StatusCode::Internal, ec));
    }
    acc->listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) {
        return core::fail(
            make_error(core::StatusCode::Internal, ec));
    }

    const auto bound = acc->local_endpoint();
    acceptor_ = std::move(acc);

    // Recursive accept loop, lives on worker 0.
    auto handler_sp = std::make_shared<SessionHandler>(std::move(handler));
    auto self       = this;

    std::function<void()> do_accept;
    auto accept_state =
        std::make_shared<std::function<void()>>();  // self-referencing
    *accept_state = [self, handler_sp, inbox_capacity, accept_state]() mutable {
        if (!self->acceptor_) return;
        self->acceptor_->async_accept(
            [self, handler_sp, inbox_capacity, accept_state](
                boost::system::error_code aec,
                boost::asio::ip::tcp::socket sock) mutable {
                if (aec) {
                    // Acceptor closed or fatal error → stop loop.
                    return;
                }
                // Migrate the OS handle onto a chosen worker's
                // executor: release the native fd from the
                // worker-0-bound socket and rewrap on the target.
                Worker& target = self->pick_worker_round_robin();
                const auto proto      = sock.local_endpoint().protocol();
                // MSVC marks socket::release() as C4996 because it
                // returns operation_not_supported on Windows < 8.1.
                // We target Windows 10 and the legacy daemons share
                // the same minimum, so silence the deprecation.
#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable : 4996)
#endif
                const auto native_fd  = sock.release();
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif
                boost::asio::ip::tcp::socket migrated(
                    target.ctx.get_executor());
                boost::system::error_code aec2;
                migrated.assign(proto, native_fd, aec2);
                if (aec2) {
                    // Best-effort: drop the connection.
                    boost::asio::post(target.ctx, [native_fd] {
                        close_native_socket(native_fd);
                    });
                } else {
                    // Hand off creation+spawn into the target's
                    // executor so all session work happens there.
                    auto migrated_sp =
                        std::make_shared<boost::asio::ip::tcp::socket>(
                            std::move(migrated));
                    auto handler_copy = *handler_sp;
                    boost::asio::post(target.ctx,
                        [migrated_sp, handler_copy = std::move(handler_copy),
                         inbox_capacity, &target]() mutable {
                            auto sess = TcpSession::create(
                                std::move(*migrated_sp));
                            auto chan = std::make_shared<
                                fiber::SessionChannel>(sess, inbox_capacity);
                            sess->set_on_bytes(
                                [chan](core::ByteView v) {
                                    chan->push_from_network(
                                        std::vector<std::byte>(
                                            v.begin(), v.end()));
                                });
                            sess->set_on_close(
                                [chan](const boost::system::error_code&) {
                                    chan->close_inbox();
                                });
                            // Spawn the handler fiber on this worker.
                            boost::fibers::fiber(
                                [chan, handler_copy = std::move(handler_copy)]() mutable {
                                    handler_copy(*chan);
                                }).detach();
                            sess->start();
                        });
                }
                // Re-arm.
                (*accept_state)();
            });
    };
    (*accept_state)();

    return bound;
}

void FiberPool::close_acceptor() noexcept {
    if (!acceptor_) return;
    boost::system::error_code ec;
    acceptor_->close(ec);
    acceptor_.reset();
}

}  // namespace pvpgn::infra::net

#endif  // PVPGN_V3_HAVE_FIBER
