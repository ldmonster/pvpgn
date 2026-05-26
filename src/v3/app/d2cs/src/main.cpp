// SPDX-License-Identifier: GPL-2.0-or-later

/// @file main.cpp
/// Entry point for the v3 D2CS server binary (`pvpgn_v3_d2cs`).
///
/// Composition root — wires the D2CS protocol FSM, application handler,
/// and in-memory repositories together with a Boost.Asio event loop.
///
/// Startup sequence
/// ----------------
///   1. Parse command-line arguments (--config, --port, --listen, --threads).
///   2. Build `D2CSConfig` (defaults + CLI overrides).
///   3. Create `IoRuntime` (Asio thread pool).
///   4. Create `TcpListener` on port 6113 (D2CS default).
///   5. Accept connections → create `D2CSTcpSession` for each.
///   6. Install SIGINT/SIGTERM handlers → IoRuntime::stop().
///   7. Run IoRuntime (blocks until stop()).
///   8. Graceful shutdown: stop listener, wait for sessions to drain.
///
/// Port
/// ----
/// D2CS listens on TCP port 6113 by default (configurable via --port).
/// Each accepted connection gets its own `D2CSTcpSession` which owns:
///   - `InMemoryCharacterRepository`  (placeholder — real FS repo later)
///   - `InMemoryLadderRepository`     (placeholder — real FS repo later)
///   - `D2CSSessionHandler`           (application layer)
///   - `D2CSSessionFsm`               (protocol layer)

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

// Boost.Asio (via system Boost — PVPGN_V3_WITH_BOOST=ON)
#include <boost/asio/ip/tcp.hpp>

// v3 infrastructure
#include "infra/net/io_runtime.hpp"
#include "infra/net/tcp_acceptor.hpp"
#include "infra/net/tcp_session.hpp"

// D2CS composition root helpers
#include "app/d2cs/d2cs_tcp_session.hpp"

// bnetd TcpListener (reused — lives in app/bnetd headers)
#include "app/bnetd/tcp_listener.hpp"

namespace pvpgn::app::d2cs {

// ---------------------------------------------------------------------------
// D2CSConfig — typed configuration snapshot
// ---------------------------------------------------------------------------

struct D2CSConfig {
    std::string   listen_address{"0.0.0.0"};
    std::uint16_t d2cs_port{6113};
    std::string   log_level{"info"};
    std::uint32_t worker_threads{0};
    std::filesystem::path data_dir{"."};
};

// ---------------------------------------------------------------------------
// Command-line parsing
// ---------------------------------------------------------------------------

struct CliArgs {
    std::string   config_path;
    std::string   listen_address;
    std::uint16_t port{0};
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
            args.port = static_cast<std::uint16_t>(
                std::stoul(std::string(next())));
        } else if (arg == "--listen" || arg == "-l") {
            args.listen_address = std::string(next());
        } else if (arg == "--log-level") {
            args.log_level = std::string(next());
        } else if (arg == "--threads" || arg == "-t") {
            args.threads = static_cast<std::uint32_t>(
                std::stoul(std::string(next())));
        } else if (arg == "--help" || arg == "-h") {
            std::cout <<
                "Usage: pvpgn_v3_d2cs [options]\n"
                "  --config,   -c <path>    Path to d2cs.toml\n"
                "  --port,     -p <port>    D2CS listen port (default 6113)\n"
                "  --listen,   -l <addr>    Listen address (default 0.0.0.0)\n"
                "  --log-level    <level>   Log level (default info)\n"
                "  --threads,  -t <n>       Worker threads (default hw_concurrency)\n"
                "  --version,  -V           Print version and exit\n"
                "  --help,     -h           Show this help\n";
            std::exit(0);
        } else if (arg == "--version" || arg == "-V") {
#ifndef PVPGN_VERSION
#  define PVPGN_VERSION "unknown"
#endif
            std::cout << "pvpgn_v3_d2cs " << PVPGN_VERSION << "\n";
            std::exit(0);
        }
    }
    return args;
}

// ---------------------------------------------------------------------------
// Build D2CSConfig from CLI args
// ---------------------------------------------------------------------------

static D2CSConfig build_config(const CliArgs& args) {
    D2CSConfig cfg;
    if (args.port != 0)                  cfg.d2cs_port       = args.port;
    if (!args.listen_address.empty())    cfg.listen_address  = args.listen_address;
    if (!args.log_level.empty())         cfg.log_level       = args.log_level;
    if (args.threads != 0)               cfg.worker_threads  = args.threads;
    return cfg;
}

// ---------------------------------------------------------------------------
// D2CS session factory
// ---------------------------------------------------------------------------

static void make_d2cs_session(
    std::shared_ptr<pvpgn::infra::net::TcpSession> tcp) {
    if (!tcp) return;
    auto session = D2CSTcpSession::create(std::move(tcp));
    session->start();
}

}  // namespace pvpgn::app::d2cs

// ---------------------------------------------------------------------------
// main()
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    using namespace pvpgn::app::d2cs;

    try {
        // 1. Parse CLI
        const CliArgs cli = parse_args(argc, argv);

        // 2. Build config
        const D2CSConfig cfg = build_config(cli);

        std::cout << "[d2cs] pvpgn_v3_d2cs composition root starting\n"
                  << "  listen address : " << cfg.listen_address << "\n"
                  << "  d2cs port      : " << cfg.d2cs_port      << "\n"
                  << "  log level      : " << cfg.log_level       << "\n";

        // 3. Create IoRuntime (Asio thread pool)
        pvpgn::infra::net::IoRuntime rt;

        // 4. Create TcpListener on port 6113
        //    The bnetd TcpListener is reused — it lives in app/bnetd headers
        //    and is a thin wrapper around infra::net::TcpAcceptor.
        pvpgn::app::bnetd::TcpListener d2cs_listener{
            rt,
            [](std::shared_ptr<pvpgn::infra::net::TcpSession> tcp) {
                make_d2cs_session(std::move(tcp));
            }};
        d2cs_listener.start(cfg.listen_address, cfg.d2cs_port);

        std::cout << "[d2cs] pvpgn_v3_d2cs listening on port "
                  << cfg.d2cs_port << "\n";

        // 5. Install signal handlers → graceful stop
        rt.install_signal_handlers({SIGINT, SIGTERM});

        // 6. Run (blocks until SIGINT/SIGTERM or rt.stop())
        const std::size_t n_threads =
            cfg.worker_threads > 0
                ? cfg.worker_threads
                : std::max(1u, std::thread::hardware_concurrency());

        std::cout << "[d2cs] starting " << n_threads << " worker thread(s)\n";
        rt.run(n_threads);

        // 7. Graceful shutdown
        std::cout << "[d2cs] shutting down\n";
        d2cs_listener.stop();

        std::cout << "[d2cs] stopped\n";
        return EXIT_SUCCESS;

    } catch (const std::exception& ex) {
        std::cerr << "[d2cs] fatal: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}
