// SPDX-License-Identifier: GPL-2.0-or-later
//
// PvPGN v3 bntrackd -- UDP server-tracker daemon.
//
// Modernized port of the legacy `src/bntrackd/bntrackd.cpp`:
//   * No link against legacy `common` / `compat`.
//   * `std::vector<ServerEntry>` instead of `t_list*` + `xmalloc`.
//   * Logging through `core::log_*` (LOG_INFO / LOG_WARN / LOG_ERROR /
//     LOG_DEBUG) instead of legacy `eventlog`.
//   * Sockets via direct POSIX / Winsock2 (no `compat/psock`).
//   * Option values parsed with `std::from_chars`.
//   * Tracker wire-packet decoded by hand using `std::memcpy` +
//     `ntohs` / `ntohl` so we do not depend on `common/bn_type` or
//     `common/tracker`.
//
// CLI flags, log messages and on-disk output formats are preserved
// against the legacy daemon so existing operator tooling and
// scrapers keep working.
//
// Implementation is split into focused sub-TUs that are #include'd
// directly into this anonymous namespace (same pattern as winmain.cpp):
//
//   bntrackd_types.hpp    — shared types, constants, globals
//   bntrackd_protocol.cpp — UDP wire-packet decode + string sanitisation
//   bntrackd_cli.cpp      — CLI option parsing (getprefs / usage)
//   bntrackd_output.cpp   — server-list file writer (ASCII + XML)
//   bntrackd_server.cpp   — main UDP receive loop + registry management
//   bntrackd_net.cpp      — platform networking init (WsaInit) + PID helper

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "core/format.hpp"
#include "core/logging.hpp"

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <process.h>
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif

#ifndef PVPGN_VERSION
#define PVPGN_VERSION "unknown"
#endif

namespace {

// ---------------------------------------------------------------------------
// Shared types, constants and globals
// ---------------------------------------------------------------------------
#include "bntrackd_types.hpp"

// ---------------------------------------------------------------------------
// Wire helpers: decode_packet(), fixup_str()
// ---------------------------------------------------------------------------
#include "bntrackd_protocol.cpp"

// ---------------------------------------------------------------------------
// CLI parsing: parse_uint(), parse_ushort(), usage(), getprefs()
// ---------------------------------------------------------------------------
#include "bntrackd_cli.cpp"

// ---------------------------------------------------------------------------
// Output file writer: write_outfile()
// ---------------------------------------------------------------------------
#include "bntrackd_output.cpp"

// ---------------------------------------------------------------------------
// Main socket loop: server_process()
// ---------------------------------------------------------------------------
#include "bntrackd_server.cpp"

// ---------------------------------------------------------------------------
// Networking init: WsaInit (Win32), current_pid()
// ---------------------------------------------------------------------------
#include "bntrackd_net.cpp"

}  // namespace

extern int main(int argc, char *argv[])
{
    if (argc < 1 || !argv || !argv[0]) {
        std::fprintf(stderr, "bad arguments\n");
        return EXIT_FAILURE;
    }

    getprefs(argc, argv);

    pvpgn::core::default_logger().set_level(
        g_prefs.debug ? pvpgn::core::LogLevel::Debug
                      : pvpgn::core::LogLevel::Info);

    if (g_prefs.logfile) {
        if (std::freopen(g_prefs.logfile, "a", stderr) == nullptr) {
            std::fprintf(stderr,
                "could not redirect stderr to \"%s\" (freopen: %s)\n",
                g_prefs.logfile, std::strerror(errno));
            return EXIT_FAILURE;
        }
    }

#ifdef DO_DAEMONIZE
    if (!g_prefs.foreground) {
        switch (::fork()) {
        case -1:
            LOG_ERROR("bntrackd", "could not fork (fork: {})",
                std::strerror(errno));
            return EXIT_FAILURE;
        case 0:
            break;
        default:
            return EXIT_SUCCESS;
        }
        ::close(0);
        ::close(1);
        ::close(2);
        if (::setsid() < 0) {
            // Best-effort.
        }
    }
#endif

    if (g_prefs.pidfile) {
        std::FILE *fp = std::fopen(g_prefs.pidfile, "w");
        if (!fp) {
            LOG_ERROR("bntrackd",
                "unable to open pid file \"{}\" for writing (fopen: {})",
                g_prefs.pidfile, std::strerror(errno));
            g_prefs.pidfile = nullptr;
        }
        else {
            std::fprintf(fp, "%lu", current_pid());
            if (std::fclose(fp) < 0) {
                LOG_ERROR("bntrackd",
                    "could not close pid file \"{}\" after writing"
                    " (fclose: {})",
                    g_prefs.pidfile, std::strerror(errno));
            }
        }
    }

    LOG_INFO("bntrackd", "bntrackd version " PVPGN_VERSION " process {}",
        current_pid());

#ifdef _WIN32
    WsaInit wsa;
    if (!wsa.ok()) {
        LOG_ERROR("bntrackd", "WSAStartup failed");
        return EXIT_FAILURE;
    }
#endif

    socket_t sockfd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == kInvalidSocket) {
        LOG_ERROR("bntrackd",
            "could not create UDP listen socket (socket: {})",
            std::strerror(errno));
        return EXIT_FAILURE;
    }

    sockaddr_in servaddr{};
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(g_prefs.port);
    if (::bind(sockfd, reinterpret_cast<sockaddr*>(&servaddr),
            sizeof(servaddr)) < 0) {
        LOG_ERROR("bntrackd",
            "could not bind to UDP port {} (bind: {})",
            g_prefs.port, std::strerror(errno));
        BNTRACKD_CLOSESOCKET(sockfd);
        return EXIT_FAILURE;
    }

    const int result = server_process(sockfd);
    BNTRACKD_CLOSESOCKET(sockfd);
    return result < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
