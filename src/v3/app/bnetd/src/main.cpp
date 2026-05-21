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
#include "infra/net/io_runtime.hpp"
#include "infra/net/tcp_acceptor.hpp"
#include "infra/net/tcp_session.hpp"

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
#include "app/bnetd/bnftp_tcp_session.hpp"
#include "app/bnetd/file_session_factory.hpp"
#include "app/bnetd/irc_session_factory.hpp"
#include "app/bnetd/irc_tcp_session.hpp"
#include "app/bnetd/server_config.hpp"
#include "app/bnetd/session_manager.hpp"
#include "app/bnetd/tcp_listener.hpp"
#include "app/bnetd/tcp_session.hpp"

namespace pvpgn::app::bnetd {

// ---------------------------------------------------------------------------
// Session-ID generator
// ---------------------------------------------------------------------------

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
// BNet+BNFTP shared-port dispatch factory
// ---------------------------------------------------------------------------
// Port 6112 is shared by BNet (first byte 0xFF) and BNFTP (first byte 0x01).
// We peek at the first byte and route accordingly.

class BnetBnftpDispatchFactory {
public:
    BnetBnftpDispatchFactory(const ServerConfig&                       cfg,
                              SessionManager&                           session_mgr,
                              const protocol::bnet::BnetUseCaseContext& use_cases)
        : cfg_(cfg), session_mgr_(session_mgr), use_cases_(use_cases) {}

    void operator()(std::shared_ptr<infra::net::TcpSession> tcp) {
        if (!tcp) return;

        // Peek buffer: accumulate bytes until we can identify the protocol.
        auto peek_buf = std::make_shared<std::vector<std::byte>>();

        tcp->set_on_bytes([this, tcp, peek_buf](core::ByteView bv) mutable {
            peek_buf->insert(peek_buf->end(), bv.begin(), bv.end());
            if (peek_buf->empty()) return;

            const std::byte first = (*peek_buf)[0];

            if (first == static_cast<std::byte>(0xFF)) {
                // BNet protocol — rewire callbacks and replay buffered bytes
                auto egress = std::make_shared<TcpSessionEgress>(tcp);
                auto ctx    = std::make_shared<
                    protocol::bnet::BnetSessionContextImpl>(
                    next_session_id(), egress);
                const domain::SessionId sid = next_session_id();
                session_mgr_.register_session(sid, ctx);
                auto fsm = std::make_shared<protocol::bnet::BnetFsm>(
                    ctx, use_cases_, sid);
                auto framer = std::make_shared<BnetFramer>();

                // Replay buffered bytes
                framer->feed(
                    core::ByteView{peek_buf->data(), peek_buf->size()},
                    [&fsm](protocol::bnet::ClientMessage msg) {
                        (void)fsm->handle(msg);
                    });

                // Rewire for future bytes
                tcp->set_on_bytes([fsm, framer](core::ByteView bv2) {
                    framer->feed(bv2, [&fsm](protocol::bnet::ClientMessage m) {
                        (void)fsm->handle(m);
                    });
                });
                tcp->set_on_close([this, sid](const boost::system::error_code&) {
                    session_mgr_.unregister_session(sid);
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
                "  --help,      -h          Show this help\n";
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

static protocol::bnet::BnetUseCaseContext build_use_cases() {
    // Phase 2 composition root: all use-case pointers are null.
    // The BnetFsm handles null use-cases gracefully (no-ops for domain calls).
    // Phase 3 will wire real in-memory or persistent adapters here.
    return protocol::bnet::BnetUseCaseContext{};
}

}  // namespace pvpgn::app::bnetd

// ---------------------------------------------------------------------------
// main()
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    using namespace pvpgn::app::bnetd;

    try {
        // 1. Parse CLI
        const CliArgs cli = parse_args(argc, argv);

        // 2. Build config
        const ServerConfig cfg = build_config(cli);

        std::cout << "[bnetd] v3 composition root starting\n"
                  << "  bnet/bnftp port : " << cfg.bnet_port  << "\n"
                  << "  wol port        : " << cfg.wol_port   << "\n"
                  << "  irc port        : " << cfg.irc_port   << "\n"
                  << "  data dir        : " << cfg.data_dir   << "\n"
                  << "  log level       : " << cfg.log_level  << "\n";

        // 3. Create IoRuntime
        pvpgn::infra::net::IoRuntime rt;

        // 4. Create SessionManager
        SessionManager session_mgr;

        // 5. Build use-case context (null stubs for Phase 2)
        auto use_cases = build_use_cases();

        // 6. Create listeners
        //
        // Port 6112: BNet + BNFTP (shared port, first-byte dispatch)
        TcpListener bnet_listener{
            rt,
            BnetBnftpDispatchFactory{cfg, session_mgr, use_cases}};
        bnet_listener.start(cfg.listen_address, cfg.bnet_port);
        std::cout << "[bnetd] BNet/BNFTP listening on "
                  << cfg.listen_address << ":" << cfg.bnet_port << "\n";

        // Dedicated BNFTP-only port (when bnftp_port differs from bnet_port).
        // Uses FileSessionFactory → BnftpTcpSession → BnftpFsm directly,
        // without the first-byte dispatch overhead of BnetBnftpDispatchFactory.
        std::optional<TcpListener> bnftp_listener;
        if (cfg.bnftp_port != cfg.bnet_port) {
            bnftp_listener.emplace(rt, FileSessionFactory{cfg.data_dir});
            bnftp_listener->start(cfg.listen_address, cfg.bnftp_port);
            std::cout << "[bnetd] BNFTP-only listening on "
                      << cfg.listen_address << ":" << cfg.bnftp_port << "\n";
        }

        // Port 4000: WOL chat
        TcpListener wol_listener{
            rt,
            [&cfg](std::shared_ptr<pvpgn::infra::net::TcpSession> tcp) {
                make_wol_session(std::move(tcp), cfg);
            }};
        wol_listener.start(cfg.listen_address, cfg.wol_port);
        std::cout << "[bnetd] WOL listening on "
                  << cfg.listen_address << ":" << cfg.wol_port << "\n";

        // Port 6667: IRC — IrcFsm wired via IrcSessionFactory
        TcpListener irc_listener{rt, IrcSessionFactory{cfg.server_name}};
        irc_listener.start(cfg.listen_address, cfg.irc_port);
        std::cout << "[bnetd] IRC listening on "
                  << cfg.listen_address << ":" << cfg.irc_port << "\n";

        // 7. Install signal handlers → graceful stop
        rt.install_signal_handlers({SIGINT, SIGTERM});

        // 8. Run (blocks until SIGINT/SIGTERM or rt.stop())
        const std::size_t n_threads =
            cfg.worker_threads > 0
                ? cfg.worker_threads
                : std::max(1u, std::thread::hardware_concurrency());

        std::cout << "[bnetd] starting " << n_threads << " worker thread(s)\n";
        rt.run(n_threads);

        // 9. Graceful shutdown
        std::cout << "[bnetd] shutting down\n";
        bnet_listener.stop();
        if (bnftp_listener) bnftp_listener->stop();
        wol_listener.stop();
        irc_listener.stop();

        std::cout << "[bnetd] stopped\n";
        return EXIT_SUCCESS;

    } catch (const std::exception& ex) {
        std::cerr << "[bnetd] fatal: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}
