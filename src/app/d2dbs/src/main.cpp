// SPDX-License-Identifier: GPL-2.0-or-later

/// @file main.cpp
/// Entry point for the v3 D2DBS server binary (`pvpgn_v3_d2dbs`).
///
/// Composition root — wires the D2DBS protocol FSM + application handler +
/// in-memory repositories to a Boost.Asio TCP listener. Each accepted
/// connection (a D2GS) gets its own `D2DBSTcpSession`.
///
/// Startup: parse CLI -> IoRuntime -> TcpListener -> accept -> session.
/// The D2GS opens with a 0x65 connect-class byte, then framed packets.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include <boost/asio/ip/tcp.hpp>

#include "infra/net/io_runtime.hpp"
#include "infra/net/tcp_session.hpp"

#include "app/d2dbs/d2dbs_tcp_session.hpp"
#include "app/bnetd/tcp_listener.hpp"

namespace pvpgn::app::d2dbs {

struct D2dbsConfig {
    std::string   listen_address{"0.0.0.0"};
    std::uint16_t port{6114};
    std::uint32_t worker_threads{0};
};

static D2dbsConfig parse_args(int argc, char* argv[]) {
    D2dbsConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        auto next = [&]() -> std::string_view {
            if (i + 1 < argc) return argv[++i];
            throw std::runtime_error(std::string("missing value for ") +
                                     std::string(arg));
        };
        if (arg == "--port" || arg == "-p") {
            cfg.port = static_cast<std::uint16_t>(std::stoul(std::string(next())));
        } else if (arg == "--listen" || arg == "-l") {
            cfg.listen_address = std::string(next());
        } else if (arg == "--threads" || arg == "-t") {
            cfg.worker_threads =
                static_cast<std::uint32_t>(std::stoul(std::string(next())));
        } else if (arg == "--help" || arg == "-h") {
            std::cout <<
                "Usage: pvpgn_v3_d2dbs [options]\n"
                "  --port,    -p <port>   D2DBS listen port (default 6114)\n"
                "  --listen,  -l <addr>   Listen address (default 0.0.0.0)\n"
                "  --threads, -t <n>      Worker threads (default hw_concurrency)\n"
                "  --help,    -h          Show this help\n";
            std::exit(0);
        }
    }
    return cfg;
}

static void make_d2dbs_session(
    std::shared_ptr<pvpgn::infra::net::TcpSession> tcp) {
    if (!tcp) return;
    D2DBSTcpSession::create(std::move(tcp))->start();
}

}  // namespace pvpgn::app::d2dbs

int main(int argc, char* argv[]) {
    using namespace pvpgn;
    using namespace pvpgn::app::d2dbs;

    try {
        const D2dbsConfig cfg = parse_args(argc, argv);

        std::cout << "[d2dbs] pvpgn_v3_d2dbs starting\n"
                  << "[d2dbs]   listen : " << cfg.listen_address << "\n"
                  << "[d2dbs]   port   : " << cfg.port << "\n";

        pvpgn::infra::net::IoRuntime rt;

        pvpgn::app::bnetd::TcpListener d2dbs_listener{
            rt,
            [](std::shared_ptr<pvpgn::infra::net::TcpSession> tcp) {
                make_d2dbs_session(std::move(tcp));
            },
            std::chrono::seconds{300}};
        d2dbs_listener.start(cfg.listen_address, cfg.port);

        std::cout << "[d2dbs] D2DBS listening on port " << cfg.port << "\n";
        std::cout.flush();

        rt.install_signal_handlers({SIGINT, SIGTERM});

        const std::size_t n_threads =
            cfg.worker_threads > 0
                ? cfg.worker_threads
                : std::max(1u, std::thread::hardware_concurrency());
        rt.run(n_threads);
        rt.wait();

        d2dbs_listener.stop();
        std::cout << "[d2dbs] stopped\n";
        return EXIT_SUCCESS;

    } catch (const std::exception& ex) {
        std::cerr << "[d2dbs] fatal: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}
