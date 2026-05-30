// SPDX-License-Identifier: GPL-2.0-or-later

/// @file main.cpp
/// Entry point for the v3 bnetd server binary (`bnetd`).
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
/// Sub-module layout (src/main/)
/// ------------------------------
///   bnet_framer.hpp          — BnetFramer template (TCP stream → packets)
///   cli_args.hpp/.cpp        — CliArgs struct + parse_args()
///   server_setup.hpp/.cpp    — build_config(), NullNlsCredentialStore,
///                              build_use_cases()
///   tcp_connection_context.hpp — TcpConnectionContext (IConnectionContext impl)
///   bnet_bnftp_dispatch.hpp/.cpp — BnetBnftpDispatchFactory (port 6112 mux)
///   wol_session.hpp/.cpp     — make_wol_session() (port 4000)
///
/// Note on BNet vs BNFTP port sharing
/// ------------------------------------
/// Both BNet and BNFTP historically share port 6112. The first byte from
/// the client disambiguates:
///   0xFF → BNet SID-framed binary protocol
///   0x01 → BNFTP file-transfer protocol (CLIENT_FILE_REQ)
/// The `BnetBnftpDispatchFactory` peeks at the first byte and creates the
/// appropriate FSM.

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

// R335: HTTP metrics server with health probes
#if __has_include("infra/metrics/http_metrics_server.hpp")
#  include "infra/metrics/http_metrics_server.hpp"
#  include "infra/metrics/in_memory_metrics_registry.hpp"
#  define PVPGN_V3_BNETD_HAVE_METRICS_SERVER 1
#endif
#include "infra/inmemory/channel_repository.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/game_repository.hpp"
#include "infra/inmemory/session_registry.hpp"
#include "infra/inmemory/unit_of_work_factory.hpp"
#include "services/bnetd/bnetd_service.hpp"

// R330: configurable persistence back-end
#include "infra/sqlite/unit_of_work_factory.hpp"
#if __has_include("infra/mysql/unit_of_work_factory.hpp")
#  include "infra/mysql/unit_of_work_factory.hpp"
#  define PVPGN_V3_BNETD_HAVE_MYSQL_FACTORY 1
#endif
#if __has_include("infra/postgres/unit_of_work_factory.hpp")
#  include "infra/postgres/unit_of_work_factory.hpp"
#  define PVPGN_V3_BNETD_HAVE_POSTGRES_FACTORY 1
#endif

// Lua runtime (infra_lua — no-op stub when Lua is not available)
#include "infra/lua/lua_runtime.hpp"

// ---------------------------------------------------------------------------
// Sub-module headers (src/main/)
// ---------------------------------------------------------------------------
#include "main/bnet_framer.hpp"
#include "main/cli_args.hpp"
#include "main/server_setup.hpp"
#include "main/tcp_connection_context.hpp"
#include "main/bnet_bnftp_dispatch.hpp"
#include "main/wol_session.hpp"

namespace pvpgn::app::bnetd {

// ---------------------------------------------------------------------------
// Global LuaRuntime — shared across all sessions.
// Initialised in main() after config is built.  All LuaConnectionContext
// instances hold a non-owning reference to this runtime.
// Declared extern in bnet_bnftp_dispatch.hpp so the dispatch factory can
// reference it without a circular include.
// ---------------------------------------------------------------------------
infra::lua::LuaRuntime g_lua_runtime;

// ---------------------------------------------------------------------------
// Session-ID generator
// ---------------------------------------------------------------------------
static std::atomic<std::uint64_t> g_next_session_id{1};

[[nodiscard]] domain::SessionId next_session_id() noexcept {
    return domain::SessionId{g_next_session_id.fetch_add(1,
                                                          std::memory_order_relaxed)};
}

} // namespace pvpgn::app::bnetd

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
            const std::string lua_main =
                (cfg.data_dir / std::filesystem::path{"lua/main.lua"}).string();
            if (auto err = g_lua_runtime.load_file(lua_main)) {
                std::cerr << "[bnetd] Lua load warning: " << *err << "\n";
                // Non-fatal: server continues without Lua scripting.
            } else {
                LOG_INFO("bnetd", "Lua runtime initialised ({})", lua_main);
                // Fire the server-start hook.
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
        // R330: Select the IUnitOfWorkFactory implementation based on the
        // [persistence] section in bnetd.toml (backend + dsn).
        // Defaults to "sqlite" when no config file is provided.

        // Load persistence config from TOML if available
        std::string persistence_backend = "sqlite";
        std::string persistence_dsn;
#if defined(PVPGN_V3_BNETD_HAVE_LOGGER_FACTORY)
        {
            if (!cli.config_path.empty()) {
                auto result = infra::config::load_server_config(cli.config_path);
                if (result.has_value()) {
                    persistence_backend = result.value().persistence.backend;
                    persistence_dsn     = result.value().persistence.dsn.reveal();
                }
            }
        }
#endif

        // Construct the appropriate UoW factory
        std::unique_ptr<application::ports::IUnitOfWorkFactory> owned_uow_factory;
        if (persistence_backend == "inmemory") {
            owned_uow_factory = std::make_unique<infra::inmemory::InMemoryUnitOfWorkFactory>();
            LOG_INFO("bnetd", "persistence backend: inmemory");
        } else if (persistence_backend == "mysql") {
#if defined(PVPGN_V3_BNETD_HAVE_MYSQL_FACTORY) && defined(PVPGN_V3_WITH_MYSQL)
            owned_uow_factory = std::make_unique<infra::mysql::MySQLUnitOfWorkFactory>(
                persistence_dsn);
            LOG_INFO("bnetd", "persistence backend: mysql dsn={}", persistence_dsn);
#else
            throw std::runtime_error(
                "persistence backend 'mysql' requested but MySQL support was not compiled in");
#endif
        } else if (persistence_backend == "postgres") {
#if defined(PVPGN_V3_BNETD_HAVE_POSTGRES_FACTORY) && defined(PVPGN_V3_WITH_POSTGRESQL)
            owned_uow_factory = std::make_unique<infra::postgres::PostgreSQLUnitOfWorkFactory>(
                persistence_dsn);
            LOG_INFO("bnetd", "persistence backend: postgres dsn={}", persistence_dsn);
#else
            throw std::runtime_error(
                "persistence backend 'postgres' requested but PostgreSQL support was not compiled in");
#endif
        } else {
            // Default: sqlite
            if (persistence_dsn.empty()) persistence_dsn = "pvpgn.db";
            owned_uow_factory = std::make_unique<infra::sqlite::SQLiteUnitOfWorkFactory>(
                persistence_dsn);
            LOG_INFO("bnetd", "persistence backend: sqlite dsn={}", persistence_dsn);
        }
        application::ports::IUnitOfWorkFactory& uow_factory = *owned_uow_factory;

        infra::inmemory::InMemoryChannelRepository  channel_repo;
        infra::inmemory::InMemoryAccountRepository  account_repo;
        infra::inmemory::InMemorySessionRegistry    session_reg;
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

        // 8a. Start HTTP metrics server with health probes (R335)
#if defined(PVPGN_V3_BNETD_HAVE_METRICS_SERVER)
        auto metrics_registry =
            std::make_shared<infra::metrics::InMemoryMetricsRegistry>();
        infra::metrics::HttpMetricsServer metrics_server{
            cfg.listen_address, cfg.admin_port, metrics_registry, rt};
        metrics_server.start();
        LOG_INFO("bnetd", "admin HTTP server listening on {}:{}",
                 cfg.listen_address, cfg.admin_port);
#endif

        // 8b. Signal readiness — all listeners are up (R335)
#if defined(PVPGN_V3_BNETD_HAVE_METRICS_SERVER)
        metrics_server.set_ready(true);
        LOG_INFO("bnetd", "server ready (GET /readyz → 200)");
#endif

        // 9. Run (blocks until SIGINT/SIGTERM or rt.stop())
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
