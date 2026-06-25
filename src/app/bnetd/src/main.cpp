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
#include "core/trace.hpp"
#include "domain/connection/connection_context.hpp"
#include "application/connection/connection_fsm.hpp"
#include "infra/net/io_runtime.hpp"
#include "infra/net/tcp_acceptor.hpp"
#include <chrono>
#include "infra/net/tcp_session.hpp"
#include "application/persistence/unit_of_work_factory.hpp"
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
#include "app/bnetd/logging_connection_context.hpp"
#include "app/bnetd/lua_connection_context.hpp"
#include "app/bnetd/server_config.hpp"
#include "app/bnetd/session_manager.hpp"
#include "app/bnetd/tcp_listener.hpp"
#include "app/bnetd/tcp_session.hpp"

// In-memory repositories + BnetdService composition root
#include "infra/inmemory/account_repository.hpp"

// HTTP metrics server with health probes
#if __has_include("infra/metrics/http_metrics_server.hpp")
#  include "infra/metrics/http_metrics_server.hpp"
#  include "infra/metrics/in_memory_metrics_registry.hpp"
#  define PVPGN_V3_BNETD_HAVE_METRICS_SERVER 1
#endif
#include "infra/inmemory/channel_repository.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/game_repository.hpp"
#include "infra/inmemory/ip_ban_repository.hpp"
#include "infra/inmemory/session_registry.hpp"
#include "infra/inmemory/srp3_credential_store.hpp"
#include "infra/inmemory/wol_credential_store.hpp"
#include "infra/inmemory/friend_list_repository.hpp"
#include "infra/inmemory/ignore_store.hpp"
#include "application/social/add_friend.hpp"
#include "application/social/remove_friend.hpp"
#include "application/social/list_friends.hpp"
#include "application/game/start_game.hpp"
#include "application/game/list_public_games.hpp"
#include "infra/inmemory/unit_of_work_factory.hpp"
#include "services/bnetd/bnetd_service.hpp"

// Wire the real auth use-cases (login + OLS account creation) so login
// enforces credentials and a real account_id flows into the chat path.
#include "application/auth/create_account.hpp"
#include "application/auth/login_user.hpp"
#include "application/auth/login_user_w3.hpp"
#include "infra/crypto/bnet_session_hasher.hpp"
#include "infra/routing/message_router.hpp"
#include "core/clock.hpp"
// Durable file-backed account store (used when backend="file").
#include "infra/file/account_repository.hpp"

// Configurable persistence back-end. Each backend is compiled in only
// when its infra target is linked (which propagates the header's include dir),
// detected here via __has_include. SQLite is the default backend but is itself
// optional — it is skipped when the sqlite3 dev headers are absent (see
// PVPGN_V3_HAVE_SQLITE3 in src/CMakeLists.txt).
#if __has_include("infra/sqlite/unit_of_work_factory.hpp")
#  include "infra/sqlite/unit_of_work_factory.hpp"
#  define PVPGN_V3_BNETD_HAVE_SQLITE_FACTORY 1
#endif
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

            // 2c. Observability: apply the head-sampling
            // ratio from [observability].sample_ratio to the process-wide
            // tracer. The default 1.0 preserves today's "every span" behaviour;
            // production lowers it (default 0.05) to bound the exporter queue.
            // This is safe with no span sink installed — the sink simply never
            // fires. Installing the concrete OTLP/HTTP span sink when
            // otlp_endpoint is set is the remaining exporter follow-up.
            const auto& obs = infra_cfg.observability;
            core::trace::set_sample_ratio(obs.sample_ratio);
            if (!obs.otlp_endpoint.empty()) {
                LOG_INFO("bnetd",
                    "observability: service={} otlp_endpoint={} sample_ratio={} "
                    "(traces sampled; OTLP span export pending exporter)",
                    obs.service_name, obs.otlp_endpoint, obs.sample_ratio);
            } else {
                LOG_INFO("bnetd",
                    "observability: local-only (no otlp_endpoint); sample_ratio={}",
                    obs.sample_ratio);
            }
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

        // 4. Create IoRuntime backed by the same io_context so that all
        //    existing TcpListener / TcpSession / TcpAcceptor code continues
        //    to work without modification.
        pvpgn::infra::net::IoRuntime rt;

        // 5. Create SessionManager
        SessionManager session_mgr;

        // 6. Build use-case context (real chat use-cases via BnetdService)
        //
        // Select the IUnitOfWorkFactory implementation based on the
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
            // Default: sqlite (when compiled in).
#if defined(PVPGN_V3_BNETD_HAVE_SQLITE_FACTORY)
            if (persistence_dsn.empty()) persistence_dsn = "pvpgn.db";
            owned_uow_factory = std::make_unique<infra::sqlite::SQLiteUnitOfWorkFactory>(
                persistence_dsn);
            LOG_INFO("bnetd", "persistence backend: sqlite dsn={}", persistence_dsn);
#else
            // SQLite backend was not compiled in (sqlite3 dev headers absent at
            // build time). Fall back to the non-durable in-memory factory so the
            // server still starts on a minimal dev box; warn loudly.
            owned_uow_factory = std::make_unique<infra::inmemory::InMemoryUnitOfWorkFactory>();
            LOG_WARN("bnetd",
                "persistence backend 'sqlite' was not compiled in (sqlite3 dev "
                "headers absent at build time); falling back to non-durable "
                "inmemory storage. Rebuild with libsqlite3-dev for durable data.");
#endif
        }
        application::ports::IUnitOfWorkFactory& uow_factory = *owned_uow_factory;

        infra::inmemory::InMemoryChannelRepository  channel_repo;
        infra::inmemory::InMemorySessionRegistry    session_reg;
        infra::inmemory::InMemoryGameRepository     game_repo;
        infra::inmemory::InMemoryEventBus           event_bus;
        infra::inmemory::InMemoryIpBanRepository    ip_ban_repo;
        // WarCraft III SRP-3 salt/verifier store (SID_AUTH_ACCOUNTCREATE writes,
        // SID_AUTH_ACCOUNTLOGON reads). Lives for the whole run loop.
        infra::inmemory::InMemorySrp3CredentialStore srp3_store;
        // Westwood Online APGAR token store (WOL CVERS/APGAR login auto-creates
        // and verifies). Lives for the whole run loop, like srp3_store.
        infra::inmemory::InMemoryWolCredentialStore  wol_store;
        // Friends list store (SID_FRIENDSLIST + /friends add|remove). Run-loop
        // scoped like the credential stores above.
        infra::inmemory::InMemoryFriendListRepository friend_repo;
        // Per-account squelch/ignore lists (/squelch /unsquelch + broadcast
        // filtering). Run-loop scoped like the stores above.
        infra::inmemory::InMemoryIgnoreStore         ignore_store;
        core::SystemClock                           auth_clock;
        NullNlsCredentialStore                      null_nls_store;

        // Account store: durable file-backed repository when the operator
        // selects [persistence] backend="file" (accounts persist as
        // <data-dir>/<name>.plain and survive restart), otherwise the
        // non-durable in-memory repository. The same instance is shared by
        // BnetdService and the auth use-cases so create/login/join all see
        // one consistent store.
        std::unique_ptr<domain::identity::IAccountRepository> owned_account_repo;
        if (persistence_backend == "file") {
            owned_account_repo = std::make_unique<infra::file::FileAccountRepository>(
                cfg.data_dir.string());
            LOG_INFO("bnetd", "account store: file ({})", cfg.data_dir.string());
        } else {
            owned_account_repo =
                std::make_unique<infra::inmemory::InMemoryAccountRepository>();
        }
        domain::identity::IAccountRepository& account_repo = *owned_account_repo;

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

        // Wire the real auth use-cases. The use-cases hold references to
        // the repositories / clock above, all of which live for the duration
        // of this scope (the server run loop), so the no-op-deleter shared_ptrs
        // and the make_shared use-cases never outlive their dependencies.
        {
            auto no_delete_accounts = std::shared_ptr<domain::identity::IAccountRepository>(
                &account_repo, [](domain::identity::IAccountRepository*) noexcept {});
            auto no_delete_sessions = std::shared_ptr<domain::identity::ISessionRegistry>(
                &session_reg, [](domain::identity::ISessionRegistry*) noexcept {});

            use_cases.account_repo     = no_delete_accounts;
            use_cases.session_registry = no_delete_sessions;
            // Stateless, thread-safe production hasher for the OLS session-hash
            // (double-hash) login path; outlives the use-cases.
            static const infra::crypto::BnetSessionHasher session_hasher;
            use_cases.login_user = std::make_shared<application::auth::LoginUser>(
                account_repo, session_reg, event_bus, auth_clock, session_hasher);
            use_cases.create_account = std::make_shared<application::auth::CreateAccount>(
                account_repo, ip_ban_repo, event_bus, auth_clock);
            // WarCraft III SRP-3 login (SID_AUTH_ACCOUNTLOGON/PROOF) + the
            // credential store written by SID_AUTH_ACCOUNTCREATE.
            use_cases.srp3_store = std::shared_ptr<application::auth::ISrp3CredentialStore>(
                &srp3_store, [](application::auth::ISrp3CredentialStore*) noexcept {});
            use_cases.login_user_w3 =
                std::make_shared<application::auth::LoginUserW3>(srp3_store);

            // Friends list (SID_FRIENDSLIST/FRIENDINFO + /friends add|remove).
            auto no_delete_friends =
                std::shared_ptr<domain::social::IFriendListRepository>(
                    &friend_repo,
                    [](domain::social::IFriendListRepository*) noexcept {});
            auto no_delete_bus = std::shared_ptr<application::ports::IEventBus>(
                &event_bus, [](application::ports::IEventBus*) noexcept {});
            auto no_delete_reader =
                std::shared_ptr<domain::identity::IAccountReader>(
                    &account_repo,
                    [](domain::identity::IAccountReader*) noexcept {});
            use_cases.add_friend = std::make_shared<application::social::AddFriend>(
                no_delete_accounts, no_delete_friends, no_delete_bus);
            use_cases.remove_friend =
                std::make_shared<application::social::RemoveFriend>(
                    no_delete_friends, no_delete_bus);
            use_cases.list_friends =
                std::make_shared<application::social::ListFriends>(
                    no_delete_friends, no_delete_sessions, no_delete_reader);

            // Hosted-game advertisement (SID_STARTADVEX3 0x1C) + game list
            // (SID_GETADVLISTEX 0x09), over the shared game repository so a game
            // hosted on one connection is visible to others.
            auto no_delete_games =
                std::shared_ptr<domain::gameplay::IGameRepository>(
                    &game_repo,
                    [](domain::gameplay::IGameRepository*) noexcept {});
            use_cases.start_game =
                std::make_shared<application::game::StartGame>(game_repo);
            use_cases.list_public_games =
                std::make_shared<application::game::ListPublicGames>(no_delete_games);

            // Squelch/ignore list (/squelch /unsquelch + broadcast filtering).
            use_cases.ignore_store =
                std::shared_ptr<application::chat::IIgnoreStore>(
                    &ignore_store,
                    [](application::chat::IIgnoreStore*) noexcept {});
        }
        LOG_INFO("bnetd", "auth use-cases wired: login + OLS account creation + W3 SRP-3");

        // Cross-session message router: maps SessionId -> egress so chat
        // broadcasts (EID_TALK, EID_JOIN/LEAVE, whispers) actually reach OTHER
        // connected clients. Without it broadcast_chat_event is a no-op and
        // channel messages are delivered to nobody. Sessions register their
        // egress with it in the dispatch (below); PostMessage/JoinChannel emit
        // the recipient SessionIds via session_reg.
        auto router_session_reg = std::shared_ptr<domain::identity::ISessionRegistry>(
            &session_reg, [](domain::identity::ISessionRegistry*) noexcept {});
        auto message_router =
            std::make_shared<infra::routing::MessageRouterImpl>(router_session_reg);
        use_cases.message_router = message_router;

        // 7. Create listeners
        //
        // Per-protocol idle-read deadlines from [net.timeouts].
        // Defaults match infra::config::NetTimeoutsConfig; overridden by TOML.
        std::chrono::milliseconds bnet_idle{std::chrono::seconds{300}};
        std::chrono::milliseconds bnftp_idle{std::chrono::seconds{60}};
        std::chrono::milliseconds wol_idle{std::chrono::seconds{300}};
        std::chrono::milliseconds irc_idle{std::chrono::seconds{300}};
#if defined(PVPGN_V3_BNETD_HAVE_LOGGER_FACTORY)
        if (!cli.config_path.empty()) {
            if (auto r = infra::config::load_server_config(cli.config_path); r.has_value()) {
                const auto& nt = r.value().net_timeouts;
                bnet_idle  = std::chrono::seconds{nt.bnet};
                bnftp_idle = std::chrono::seconds{nt.bnftp};
                wol_idle   = std::chrono::seconds{nt.wol};
                irc_idle   = std::chrono::seconds{nt.irc};
            }
        }
#endif

        // Port 6112: BNet + BNFTP (shared port, first-byte dispatch)
        TcpListener bnet_listener{
            rt,
            BnetBnftpDispatchFactory{cfg, session_mgr, use_cases, bnetd_svc, message_router},
            bnet_idle};
        bnet_listener.start(cfg.listen_address, cfg.bnet_port);
        LOG_INFO("bnetd", "BNet/BNFTP listening on {}:{}", cfg.listen_address, cfg.bnet_port);

        // Dedicated BNFTP-only port (when bnftp_port differs from bnet_port).
        std::optional<TcpListener> bnftp_listener;
        if (cfg.bnftp_port != cfg.bnet_port) {
            bnftp_listener.emplace(rt, FileSessionFactory{cfg.data_dir}, bnftp_idle);
            bnftp_listener->start(cfg.listen_address, cfg.bnftp_port);
            LOG_INFO("bnetd", "BNFTP-only listening on {}:{}", cfg.listen_address, cfg.bnftp_port);
        }

        // Port 4000: WOL chat. Wire the native Westwood auth collaborators so
        // WOL clients log in via the CVERS/APGAR flow (auto-create on first
        // login, verbatim APGAR compare thereafter) — matching the original.
        protocol::wol::WolAuthDeps wol_auth{
            /* create_account   = */ use_cases.create_account.get(),
            /* account_reader   = */ &account_repo,
            /* wol_store        = */ &wol_store,
            /* session_registry = */ &session_reg,
        };
        auto wol_list_channels = use_cases.list_channels;
        auto wol_join_channel  = use_cases.join_channel;
        auto wol_post_message  = use_cases.post_message;
        TcpListener wol_listener{
            rt,
            [&cfg, wol_auth, wol_list_channels, wol_join_channel,
             wol_post_message](std::shared_ptr<pvpgn::infra::net::TcpSession> tcp) {
                make_wol_session(std::move(tcp), cfg, wol_auth,
                                 wol_list_channels, wol_join_channel,
                                 wol_post_message);
            },
            wol_idle};
        wol_listener.start(cfg.listen_address, cfg.wol_port);
        LOG_INFO("bnetd", "WOL listening on {}:{}", cfg.listen_address, cfg.wol_port);

        // Port 6667: IRC — IrcFsm wired via IrcSessionFactory
        TcpListener irc_listener{rt, IrcSessionFactory{cfg.server_name}, irc_idle};
        irc_listener.start(cfg.listen_address, cfg.irc_port);
        LOG_INFO("bnetd", "IRC listening on {}:{}", cfg.listen_address, cfg.irc_port);

        // 8. Install signal handlers → graceful stop
        rt.install_signal_handlers({SIGINT, SIGTERM});

        // 8a. Start HTTP metrics server with health probes
#if defined(PVPGN_V3_BNETD_HAVE_METRICS_SERVER)
        auto metrics_registry =
            std::make_shared<infra::metrics::InMemoryMetricsRegistry>();
        infra::metrics::HttpMetricsServer metrics_server{
            cfg.listen_address, cfg.admin_port, metrics_registry, rt};
        metrics_server.start();
        LOG_INFO("bnetd", "admin HTTP server listening on {}:{}",
                 cfg.listen_address, cfg.admin_port);
#endif

        // 8b. Signal readiness — all listeners are up
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
        // run() only spawns the workers; block here until a SIGINT/SIGTERM
        // (or rt.request_stop()) unblocks the io_context, then fall through to
        // graceful shutdown. Without this the daemon would exit immediately.
        rt.wait();

        // 10. Graceful shutdown
        LOG_INFO("bnetd", "shutting down");
        bnet_listener.stop();
        if (bnftp_listener) bnftp_listener->stop();
        wol_listener.stop();
        irc_listener.stop();

        LOG_INFO("bnetd", "stopped");
        return EXIT_SUCCESS;

    } catch (const std::exception& ex) {
        std::cerr << "[bnetd] fatal: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}
