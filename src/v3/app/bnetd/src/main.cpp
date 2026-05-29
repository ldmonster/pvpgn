// SPDX-License-Identifier: GPL-2.0-or-later

/// @file main.cpp
/// Entry point for the v3 bnetd server binary (`pvpgn_v3_bnetd`).
///
/// Composition root — wires all v3 FSMs together with a Boost.Asio event loop.
///
/// Startup sequence
/// ----------------
///   1. Parse command-line arguments (--config, --port, --data-dir,
///      --log-level, --threads).
///   2. Build `ServerConfig` (defaults + CLI overrides).
///   3. Create `IoRuntime` (Asio thread pool).
///   4. Create `SessionManager`.
///   5. Create `TcpListener` for each protocol port:
///        - BNet  :6112  → BnetFsm
///        - BNFTP :6112  → BnftpFsm  (same port, different first byte)
///        - WOL   :4000  → WolFsm
///        - IRC   :6667  → IrcFsm    (protocol_irc)
///   6. Install SIGINT/SIGTERM handlers → IoRuntime::stop().
///   7. Run IoRuntime (blocks until stop()).
///   8. Graceful shutdown: stop all listeners, wait for sessions to drain.
///
/// Note on BNet vs BNFTP port sharing
/// ------------------------------------
/// Both BNet and BNFTP historically share port 6112. The first byte from
/// the client disambiguates:
///   0xFF → BNet SID-framed binary protocol
///   0x01 → BNFTP file-transfer protocol (CLIENT_FILE_REQ)
/// The `BnetBnftpDispatchFactory` below peeks at the first byte and
/// creates the appropriate FSM.
///
/// Note on IRC
/// -----------
/// `protocol_irc` exists in the build but has no `IrcFsm` header in the
/// standard location. The IRC listener is conditionally compiled only when
/// the `protocol_irc` target provides the expected header.

#include <atomic>
#include <algorithm>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Boost.Asio (via system Boost — PVPGN_V3_WITH_BOOST=ON)
#include <boost/asio/ip/tcp.hpp>

// v3 infrastructure
#include "core/bytes.hpp"
#include "core/format.hpp"
#include "domain/connection/connection_context.hpp"
#include "application/connection/connection_fsm.hpp"
#include "infra/net/io_runtime.hpp"
#include "infra/net/tcp_acceptor.hpp"
#include "infra/net/tcp_session.hpp"
#if defined(PVPGN_V3_BNETD_HAVE_CONFIG) && __has_include("infra/config/server_config.hpp")
#  include "infra/config/server_config.hpp"
#  include "infra/log/logger_factory.hpp"
#  define PVPGN_V3_BNETD_HAVE_LOGGER_FACTORY 1
#endif

// Protocol FSMs
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/fsm.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/bnet/session_context_impl.hpp"
#include "protocol/bnet/use_case_context.hpp"
#include "protocol/common/packet.hpp"
#include "protocol/file/bnftp_fsm.hpp"
#include "protocol/irc/codec.hpp"
#include "protocol/irc/fsm.hpp"
#include "protocol/wol/wol_fsm.hpp"

// Composition root helpers
#include "app/bnetd/asio_event_loop.hpp"
#include "app/bnetd/bnet_connection_adapter.hpp"
#include "app/bnetd/bnftp_tcp_session.hpp"
#include "app/bnetd/file_session_factory.hpp"
#include "app/bnetd/irc_session_factory.hpp"
#include "app/bnetd/irc_tcp_session.hpp"
#include "app/bnetd/legacy_bridge.hpp"
#include "app/bnetd/logging_connection_context.hpp"
#include "app/bnetd/lua_connection_context.hpp"
#include "app/bnetd/server_config.hpp"
#include "app/bnetd/session_manager.hpp"
#include "app/bnetd/tcp_listener.hpp"
#include "app/bnetd/tcp_session.hpp"

// R304/R305: in-memory repositories + BnetdService composition root
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/channel_repository.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/game_repository.hpp"
#include "infra/inmemory/session_registry.hpp"
#include "infra/inmemory/unit_of_work_factory.hpp"
#include "services/bnetd/bnetd_service.hpp"

// Lua runtime (infra_lua — no-op stub when Lua is not available)
#include "infra/lua/lua_runtime.hpp"

namespace pvpgn::app::bnetd {

// ---------------------------------------------------------------------------
// Session-ID generator
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Global LuaRuntime — shared across all sessions
// ---------------------------------------------------------------------------
// Initialised in main() after config is built.  All LuaConnectionContext
// instances hold a non-owning reference to this runtime.

static infra::lua::LuaRuntime g_lua_runtime;

static std::atomic<std::uint64_t> g_next_session_id{1};

[[nodiscard]] static domain::SessionId next_session_id() noexcept {
    return domain::SessionId{g_next_session_id.fetch_add(1,
                                                          std::memory_order_relaxed)};
}

// ---------------------------------------------------------------------------
// BNet framing buffer
// ---------------------------------------------------------------------------
// Each BNet session needs a per-connection accumulation buffer because TCP
// delivers a stream, not packets. We accumulate bytes until we have a
// complete 4-byte header + payload, then decode and feed to the FSM.

struct BnetFramer {
    std::vector<std::byte> buf;

    /// Feed raw bytes; call `fn` for each complete decoded ClientMessage.
    template <class Fn>
    void feed(core::ByteView incoming, Fn&& fn) {
        buf.insert(buf.end(), incoming.begin(), incoming.end());

        while (buf.size() >= protocol::BnetHeader::kSize) {
            auto hdr_result = protocol::parse_bnet_header(
                core::ByteView{buf.data(), buf.size()});
            if (!hdr_result) break;  // malformed — caller should close

            const std::uint16_t pkt_size = hdr_result.value().size;
            if (buf.size() < pkt_size) break;  // incomplete packet

            // We have a complete packet — parse_packet fills header + payload.
            auto fp_result = protocol::parse_packet(
                core::ByteView{buf.data(), pkt_size});
            if (!fp_result) {
                buf.erase(buf.begin(), buf.begin() + pkt_size);
                continue;
            }
            auto msg_result = protocol::bnet::decode_client(fp_result.value().packet);
            if (msg_result) {
                fn(std::move(msg_result.value()));
            }
            // Consume the packet bytes regardless of decode success.
            buf.erase(buf.begin(),
                      buf.begin() + static_cast<std::ptrdiff_t>(pkt_size));
        }
    }
};

// ---------------------------------------------------------------------------
// TcpConnectionContext — IConnectionContext backed by TcpSessionEgress
// ---------------------------------------------------------------------------
// Minimal IConnectionContext implementation that forwards send_packet/close
// to a TcpSessionEgress. Used by BnetConnectionAdapter in the composition
// root so that ConnectionFsm can send BNCS packets over the TCP transport.

class TcpConnectionContext final
    : public domain::connection::IConnectionContext {
public:
    TcpConnectionContext(std::shared_ptr<TcpSessionEgress> egress,
                         std::string                        remote_addr,
                         std::uint32_t                      session_id) noexcept
        : egress_(std::move(egress))
        , remote_addr_(std::move(remote_addr))
        , session_id_(session_id) {}

    [[nodiscard]] core::Status<> send_packet(
        std::uint8_t packet_id,
        std::span<const std::byte> payload) override {
        // Build a minimal 4-byte BNCS header + payload and send.
        // Header: 0xFF, packet_id, length (LE uint16)
        //
        // R169.e: avoid the reserve+push_back+insert pattern that
        // gcc 15 mis-diagnoses as `-Werror=free-nonheap-object` on
        // -O2. Pre-size the vector and write through indices.
        const std::size_t total = 4u + payload.size();
        const std::uint16_t total_le = static_cast<std::uint16_t>(total);
        std::vector<std::byte> buf(total);
        buf[0] = std::byte{0xFF};
        buf[1] = std::byte{packet_id};
        buf[2] = std::byte{static_cast<std::uint8_t>(total_le & 0xFFu)};
        buf[3] = std::byte{static_cast<std::uint8_t>((total_le >> 8) & 0xFFu)};
        if (!payload.empty()) {
            std::copy(payload.begin(), payload.end(), buf.begin() + 4);
        }
        egress_->send(std::move(buf));
        return core::ok();
    }

    void close() override {
        egress_->close();
    }

    [[nodiscard]] std::string get_remote_address() const override {
        return remote_addr_;
    }

    [[nodiscard]] std::uint32_t get_session_id() const override {
        return session_id_;
    }

    void on_game_created(std::uint32_t /*game_id*/,
                         const domain::connection::GameInfo& /*info*/) override {
        // TODO(Phase3): wire into game registry
    }

    void on_game_joined(std::uint32_t /*game_id*/,
                        const domain::connection::GameInfo& /*info*/) override {
        // TODO(Phase3): wire into game registry
    }

    void on_game_left(std::uint32_t /*game_id*/) override {
        // TODO(Phase3): wire into game registry
    }

private:
    std::shared_ptr<TcpSessionEgress> egress_;
    std::string                        remote_addr_;
    std::uint32_t                      session_id_;
};

// ---------------------------------------------------------------------------
// BNet+BNFTP shared-port dispatch factory
// ---------------------------------------------------------------------------
// Port 6112 is shared by BNet (first byte 0xFF) and BNFTP (first byte 0x01).
// We peek at the first byte and route accordingly.

class BnetBnftpDispatchFactory {
public:
    BnetBnftpDispatchFactory(const ServerConfig&                       cfg,
                              SessionManager&                           session_mgr,
                              const protocol::bnet::BnetUseCaseContext& use_cases,
                              services::bnetd::BnetdService&            bnetd_svc)
        : cfg_(cfg), session_mgr_(session_mgr), use_cases_(use_cases)
        , bnetd_svc_(bnetd_svc) {}

    void operator()(std::shared_ptr<infra::net::TcpSession> tcp) {
        if (!tcp) return;

        // Peek buffer: accumulate bytes until we can identify the protocol.
        auto peek_buf = std::make_shared<std::vector<std::byte>>();

        tcp->set_on_bytes([this, tcp, peek_buf](core::ByteView bv) mutable {
            peek_buf->insert(peek_buf->end(), bv.begin(), bv.end());
            if (peek_buf->empty()) return;

            const std::byte first = (*peek_buf)[0];

            if (first == static_cast<std::byte>(0xFF)) {
                // BNet protocol — rewire callbacks and replay buffered bytes.
                //
                // Wiring:
                //   TcpSessionEgress
                //     ├── BnetSessionContextImpl  → BnetFsm (wire-level)
                //     └── TcpConnectionContext
                //           └── LuaConnectionContext   ← fires Lua hooks
                //                 └── LoggingConnectionContext
                //                       └── BnetConnectionAdapter
                //                             └── ConnectionFsm (domain-level)
                //
                // Both FSMs share the same TCP egress. BnetFsm handles the
                // wire dance (PING echo, auth acks). ConnectionFsm tracks
                // domain state (Connecting → Authenticating → LoggedIn → …).
                // The composition root feeds each decoded packet to both FSMs.

                auto egress = std::make_shared<TcpSessionEgress>(tcp);
                const domain::SessionId sid = next_session_id();
                const std::uint32_t     sid32 =
                    static_cast<std::uint32_t>(sid.value());

                // BnetFsm I/O context (wire-level replies)
                auto bnet_ctx = std::make_shared<
                    protocol::bnet::BnetSessionContextImpl>(sid, egress);
                session_mgr_.register_session(sid, bnet_ctx);

                // Domain-level transport context
                auto tcp_conn_ctx = std::make_shared<TcpConnectionContext>(
                    egress, /*remote_addr=*/"", sid32);

                // LuaConnectionContext fires Lua hooks; wraps tcp_conn_ctx
                auto lua_ctx = std::make_shared<LuaConnectionContext>(
                    *tcp_conn_ctx, g_lua_runtime);

                // LoggingConnectionContext wraps lua_ctx
                auto logging_ctx = std::make_shared<LoggingConnectionContext>(
                    *lua_ctx);

                // BnetConnectionAdapter owns ConnectionFsm; implements
                // IConnectionContext by forwarding to logging_ctx
                auto adapter = std::make_shared<BnetConnectionAdapter>(
                    *logging_ctx, sid32);

                auto fsm    = std::make_shared<protocol::bnet::BnetFsm>(
                    bnet_ctx, use_cases_, sid);
                auto framer = std::make_shared<BnetFramer>();

                // Replay buffered bytes through both FSMs
                framer->feed(
                    core::ByteView{peek_buf->data(), peek_buf->size()},
                    [&fsm, &adapter](protocol::bnet::ClientMessage msg) {
                        // Feed to BnetFsm (wire-level)
                        (void)fsm->handle(msg);
                        // Feed to ConnectionFsm (domain-level) via adapter.
                        // We need the raw packet_id + payload; for replay we
                        // use an empty payload since the BnetFsm already
                        // handled the wire dance. The domain FSM will silently
                        // ignore unknown SIDs.
                        // TODO(Phase3): extract packet_id from ClientMessage
                        // variant and pass the original payload bytes.
                    });

                // Rewire for future bytes
                tcp->set_on_bytes(
                    [fsm, framer, adapter](core::ByteView bv2) {
                        framer->feed(bv2,
                            [&fsm](protocol::bnet::ClientMessage m) {
                                (void)fsm->handle(m);
                            });
                    });

                // Keep tcp_conn_ctx, lua_ctx, and logging_ctx alive for the
                // session lifetime by capturing them in the close handler
                // alongside the adapter (which holds non-owning refs to all).
                // R305: bnetd_svc_ is a member of BnetBnftpDispatchFactory;
                // captured via `this` for LogoutUser cleanup on disconnect.
                tcp->set_on_close(
                    [this, sid, adapter, tcp_conn_ctx, lua_ctx, logging_ctx](
                        const boost::system::error_code&) {
                        // R305: call LogoutUser to clean up channel membership
                        // before unregistering the session.
                        const auto& conn_fsm = adapter->connection_fsm();
                        const std::uint32_t acct_id = conn_fsm.account_id();
                        if (acct_id != 0) {
                            application::auth::LogoutRequest req{
                                domain::SessionId{sid},
                                domain::AccountId{acct_id}};
                            (void)bnetd_svc_.logout_user().execute(req);
                        }
                        session_mgr_.unregister_session(sid);
                        adapter->connection_fsm().close();
                        (void)tcp_conn_ctx;
                        (void)lua_ctx;
                        (void)logging_ctx;
                    });

            } else {
                // BNFTP protocol (or unknown — let BnftpFsm reject it)
                auto egress = std::make_shared<TcpSessionEgress>(tcp);
                auto ctx    = std::make_shared<BnftpEgressContext>(egress);
                auto fsm    = std::make_shared<protocol::file::BnftpFsm>(
                    ctx, cfg_.data_dir.string());

                // Replay buffered bytes
                auto sp = std::span<const std::byte>(peek_buf->data(),
                                                      peek_buf->size());
                (void)fsm->on_bytes(sp);

                // Rewire for future bytes
                tcp->set_on_bytes([fsm](core::ByteView bv2) {
                    auto sp2 = std::span<const std::byte>(bv2.data(), bv2.size());
                    (void)fsm->on_bytes(sp2);
                });
                tcp->set_on_close([fsm](const boost::system::error_code&) {
                    fsm->on_close();
                });
            }

            // Clear peek buffer — it has been replayed
            peek_buf->clear();
        });

        tcp->start();
    }

private:
    const ServerConfig&                       cfg_;
    SessionManager&                           session_mgr_;
    protocol::bnet::BnetUseCaseContext        use_cases_;
    services::bnetd::BnetdService&            bnetd_svc_;
};

// ---------------------------------------------------------------------------
// WOL session factory
// ---------------------------------------------------------------------------

static void make_wol_session(
    std::shared_ptr<infra::net::TcpSession> tcp,
    const ServerConfig&                      cfg) {

    auto egress = std::make_shared<TcpSessionEgress>(tcp);
    auto ctx    = std::make_shared<WolEgressContext>(egress, cfg.server_name);
    auto fsm    = std::make_shared<protocol::wol::WolFsm>(ctx);

    tcp->set_on_bytes([fsm](core::ByteView bv) {
        auto sp = std::span<const std::byte>(bv.data(), bv.size());
        (void)fsm->on_bytes(sp);
    });

    tcp->set_on_close([fsm](const boost::system::error_code&) {
        fsm->on_close();
    });

    tcp->start();
}

// ---------------------------------------------------------------------------
// Command-line parsing
// ---------------------------------------------------------------------------

struct CliArgs {
    std::string   config_path;
    std::uint16_t bnet_port{0};
    std::string   data_dir;
    std::string   log_level;
    std::uint32_t threads{0};
};

static CliArgs parse_args(int argc, char* argv[]) {
    CliArgs args;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        auto next = [&]() -> std::string_view {
            if (i + 1 < argc) return argv[++i];
            throw std::runtime_error(std::string("missing value for ") +
                                     std::string(arg));
        };
        if (arg == "--config" || arg == "-c") {
            args.config_path = std::string(next());
        } else if (arg == "--port" || arg == "-p") {
            args.bnet_port = static_cast<std::uint16_t>(
                std::stoul(std::string(next())));
        } else if (arg == "--data-dir" || arg == "-d") {
            args.data_dir = std::string(next());
        } else if (arg == "--log-level" || arg == "-l") {
            args.log_level = std::string(next());
        } else if (arg == "--threads" || arg == "-t") {
            args.threads = static_cast<std::uint32_t>(
                std::stoul(std::string(next())));
        } else if (arg == "--help" || arg == "-h") {
            std::cout <<
                "Usage: pvpgn_v3_bnetd [options]\n"
                "  --config,    -c <path>   Path to bnetd.toml\n"
                "  --port,      -p <port>   BNet/BNFTP port (default 6112)\n"
                "  --data-dir,  -d <path>   Data directory (default .)\n"
                "  --log-level, -l <level>  Log level (default info)\n"
                "  --threads,   -t <n>      Worker threads (default hw_concurrency)\n"
                "  --version,   -V          Print version and exit\n"
                "  --help,      -h          Show this help\n";
            std::exit(0);
        } else if (arg == "--version" || arg == "-V") {
#ifndef PVPGN_VERSION
#  define PVPGN_VERSION "unknown"
#endif
            std::cout << "pvpgn_v3_bnetd " << PVPGN_VERSION << "\n";
            std::exit(0);
        }
    }
    return args;
}

// ---------------------------------------------------------------------------
// Build ServerConfig from CLI args (TOML loading deferred to Phase 3)
// ---------------------------------------------------------------------------

static ServerConfig build_config(const CliArgs& args) {
    ServerConfig cfg;
    if (args.bnet_port != 0)    cfg.bnet_port  = args.bnet_port;
    if (!args.data_dir.empty()) cfg.data_dir   = args.data_dir;
    if (!args.log_level.empty()) cfg.log_level = args.log_level;
    if (args.threads != 0)      cfg.worker_threads = args.threads;
    return cfg;
}

// ---------------------------------------------------------------------------
// Build a null BnetUseCaseContext (in-memory stubs for standalone mode)
// ---------------------------------------------------------------------------

/// Null NLS credential store — used when no persistent credential backend is
/// available.  Returns std::nullopt for every lookup so that WAR3/W3XP NLS
/// auth always fails gracefully (the FSM falls back to OLS or rejects).
struct NullNlsCredentialStore final
    : public application::auth::INlsCredentialStore {
    [[nodiscard]] std::optional<application::auth::NlsCredentials>
    find(std::string_view /*username*/) override {
        return std::nullopt;
    }
};

/// Build a BnetUseCaseContext populated with real chat use-cases from
/// BnetdService.  The service owns the use-cases; the context holds
/// non-owning shared_ptrs (no-op deleters) so the FSM can call them.
static protocol::bnet::BnetUseCaseContext
build_use_cases(services::bnetd::BnetdService& svc) {
    return svc.make_use_case_context();
}

}  // namespace pvpgn::app::bnetd

// ---------------------------------------------------------------------------
// main()
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    using namespace pvpgn;
    using namespace pvpgn::app::bnetd;

    try {
        // 1. Parse CLI
        const CliArgs cli = parse_args(argc, argv);

        // 2. Build config
        const ServerConfig cfg = build_config(cli);

        // 2b. Logger initialization from TOML config
#if defined(PVPGN_V3_BNETD_HAVE_LOGGER_FACTORY)
        {
            infra::config::ServerConfig infra_cfg;
            if (!cli.config_path.empty()) {
                auto result = infra::config::load_server_config(cli.config_path);
                if (result.has_value()) {
                    infra_cfg = std::move(result.value());
                } else {
                    std::cerr << "[bnetd] Warning: failed to load config from "
                              << cli.config_path << ": " << result.error().message() << "\n";
                }
            }
            infra::log::make_and_install_logger(infra_cfg.log, "bnetd");
        }
#endif
        LOG_INFO("bnetd", "pvpgn bnetd starting, config={}",
            cli.config_path.empty() ? std::string("(defaults)") : cli.config_path);
        LOG_INFO("bnetd", "  bnet/bnftp port : {}", cfg.bnet_port);
        LOG_INFO("bnetd", "  wol port        : {}", cfg.wol_port);
        LOG_INFO("bnetd", "  irc port        : {}", cfg.irc_port);
        LOG_INFO("bnetd", "  data dir        : {}", cfg.data_dir.string());
        LOG_INFO("bnetd", "  log level       : {}", cfg.log_level);

        // 2a. Initialise Lua runtime and load scripts.
        //     g_lua_runtime is a global LuaRuntime (RAII, opened in its ctor).
        //     We load lua/main.lua which in turn loads all other lua/ scripts
        //     via the legacy require/dofile chain.
        //     If Lua is not available (PVPGN_HAVE_LUA not defined) or the
        //     script directory does not exist, this is a silent no-op.
        if (g_lua_runtime.is_open()) {
            // Load the Lua entry point relative to the data directory.
            // The legacy server loads all .lua files from scriptdir; here we
            // load main.lua which is the conventional entry point.
            const std::string lua_main =
                (cfg.data_dir / std::filesystem::path{"lua/main.lua"}).string();
            if (auto err = g_lua_runtime.load_file(lua_main)) {
                std::cerr << "[bnetd] Lua load warning: " << *err << "\n";
                // Non-fatal: server continues without Lua scripting.
            } else {
                LOG_INFO("bnetd", "Lua runtime initialised ({})", lua_main);
                // Fire the server-start hook (equivalent to legacy
                // lua_handle_server(luaevent_server_start)).
                (void)g_lua_runtime.call_hook("main");
            }
        } else {
            LOG_INFO("bnetd", "Lua not available — scripting disabled");
        }

        // 3. Create AsioEventLoop (wraps io_context + work guard)
        AsioEventLoop event_loop;

        // 3a. Initialise LegacyBridge singleton so that server_tick_v3()
        //     can call event_loop.run_for() from the legacy main loop.
        LegacyBridge::init(event_loop);

        // 4. Create IoRuntime backed by the same io_context so that all
        //    existing TcpListener / TcpSession / TcpAcceptor code continues
        //    to work without modification.
        pvpgn::infra::net::IoRuntime rt;

        // 5. Create SessionManager
        SessionManager session_mgr;

        // 6. Build use-case context (R304/R305: real chat use-cases via BnetdService)
        //
        // Create in-memory repositories and a BnetdService that owns the chat
        // use-cases and seeds the channel repository with default channels.
        // A null NLS credential store is used here; WAR3/W3XP NLS auth is
        // handled separately by BnetConnectionAdapter / ConnectionFsm.
        // R305: BnetdService also owns LogoutUser (wired with LeaveChannel)
        // for channel cleanup on disconnect.
        infra::inmemory::InMemoryChannelRepository  channel_repo;
        infra::inmemory::InMemoryAccountRepository  account_repo;
        infra::inmemory::InMemorySessionRegistry    session_reg;
        infra::inmemory::InMemoryUnitOfWorkFactory  uow_factory;
        infra::inmemory::InMemoryGameRepository     game_repo;
        infra::inmemory::InMemoryEventBus           event_bus;
        NullNlsCredentialStore                      null_nls_store;

        services::bnetd::BnetdService bnetd_svc{
            uow_factory,
            event_loop,
            null_nls_store,
            channel_repo,
            account_repo,
            session_reg,
            game_repo,
            event_bus};

        auto use_cases = build_use_cases(bnetd_svc);

        // 7. Create listeners
        //
        // Port 6112: BNet + BNFTP (shared port, first-byte dispatch)
        TcpListener bnet_listener{
            rt,
            BnetBnftpDispatchFactory{cfg, session_mgr, use_cases, bnetd_svc}};
        bnet_listener.start(cfg.listen_address, cfg.bnet_port);
        LOG_INFO("bnetd", "BNet/BNFTP listening on {}:{}", cfg.listen_address, cfg.bnet_port);

        // Dedicated BNFTP-only port (when bnftp_port differs from bnet_port).
        // Uses FileSessionFactory → BnftpTcpSession → BnftpFsm directly,
        // without the first-byte dispatch overhead of BnetBnftpDispatchFactory.
        std::optional<TcpListener> bnftp_listener;
        if (cfg.bnftp_port != cfg.bnet_port) {
            bnftp_listener.emplace(rt, FileSessionFactory{cfg.data_dir});
            bnftp_listener->start(cfg.listen_address, cfg.bnftp_port);
            LOG_INFO("bnetd", "BNFTP-only listening on {}:{}", cfg.listen_address, cfg.bnftp_port);
        }

        // Port 4000: WOL chat
        TcpListener wol_listener{
            rt,
            [&cfg](std::shared_ptr<pvpgn::infra::net::TcpSession> tcp) {
                make_wol_session(std::move(tcp), cfg);
            }};
        wol_listener.start(cfg.listen_address, cfg.wol_port);
        LOG_INFO("bnetd", "WOL listening on {}:{}", cfg.listen_address, cfg.wol_port);

        // Port 6667: IRC — IrcFsm wired via IrcSessionFactory
        TcpListener irc_listener{rt, IrcSessionFactory{cfg.server_name}};
        irc_listener.start(cfg.listen_address, cfg.irc_port);
        LOG_INFO("bnetd", "IRC listening on {}:{}", cfg.listen_address, cfg.irc_port);

        // 8. Install signal handlers → graceful stop
        rt.install_signal_handlers({SIGINT, SIGTERM});

        // 9. Run (blocks until SIGINT/SIGTERM or rt.stop())
        //    The IoRuntime worker threads drive the shared io_context.
        //    The AsioEventLoop::run() call below is a secondary entry point
        //    used when the legacy loop is NOT running (pure v3 mode).
        const std::size_t n_threads =
            cfg.worker_threads > 0
                ? cfg.worker_threads
                : std::max(1u, std::thread::hardware_concurrency());

        LOG_INFO("bnetd", "starting {} worker thread(s)", n_threads);
        rt.run(n_threads);

        // 10. Graceful shutdown
        LOG_INFO("bnetd", "shutting down");
        bnet_listener.stop();
        if (bnftp_listener) bnftp_listener->stop();
        wol_listener.stop();
        irc_listener.stop();

        LegacyBridge::shutdown();

        LOG_INFO("bnetd", "stopped");
        return EXIT_SUCCESS;

    } catch (const std::exception& ex) {
        std::cerr << "[bnetd] fatal: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}
