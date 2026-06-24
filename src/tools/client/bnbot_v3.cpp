// SPDX-License-Identifier: GPL-2.0-or-later
//
// bnbot -- minimalist Battle.net "bot" telnet-style client.
//
// Connects to a bnetd-compatible server, sends the
// CLIENT_INITCONN_CLASS_BOT class byte, then forwards raw bytes
// between the terminal and the socket.  Line-edits stdin in
// non-canonical mode but does NOT echo (legacy behavior: the server
// echoes back, the user types blind).
//
// Self-contained C++20:
// no legacy `common/*` or `compat/*` headers.  Only depends on
// `core` (for logging macros) and the vendored
// `bnclient_net.hpp` / `bnclient_proto.hpp` headers in this
// directory.

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <print>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#include "core/format.hpp"
#include "core/logging.hpp"

#include "bnclient_net.hpp"
#include "bnclient_proto.hpp"

#ifdef _WIN32
#  include <conio.h>
#  include <io.h>
#else
#  include <sys/ioctl.h>
#  include <termios.h>
#  include <unistd.h>
#endif

#ifndef PVPGN_VERSION
#  define PVPGN_VERSION "unknown"
#endif

namespace {

using ::pvpgn::client_v3::net::socket_t;
using ::pvpgn::client_v3::net::kInvalidSocket;
using ::pvpgn::client_v3::net::socket_holder;
using ::pvpgn::core::default_logger;
using ::pvpgn::core::LogLevel;

constexpr std::uint16_t kDefaultPort       = 6112;     // BNETD_SERV_PORT
constexpr const char*   kDefaultHost       = "localhost";
constexpr std::size_t   kMaxLineLen        = 1024;     // MAX_MESSAGE_LEN
constexpr unsigned      kDefaultScreenW    = 80;
constexpr unsigned      kDefaultScreenH    = 25;
constexpr std::uint8_t  kInitBot           = 0x03;     // CLIENT_INITCONN_CLASS_BOT
constexpr char          kBotProtocolMarker = '\004';   // ^D, mandatory after class byte

[[noreturn]] void usage(const char* progname) {
    std::print(stderr,
        "usage: {} [<options>] [<servername> [<TCP portnumber>]]\n"
        "    -h, --help, --usage         show this information and exit\n"
        "    -v, --version               print version number and exit\n",
        progname);
    std::exit(EXIT_FAILURE);
}

// Parse a decimal port number (1..65535).  Returns std::nullopt on
// bad input.
bool parse_port(std::string_view s, std::uint16_t& out) noexcept {
    unsigned v = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || ptr != s.data() + s.size() || v == 0 || v > 0xffff) {
        return false;
    }
    out = static_cast<std::uint16_t>(v);
    return true;
}

// ---- terminal helpers --------------------------------------------------

bool query_term_size(int fd, unsigned& w, unsigned& h) noexcept {
    w = 0;
    h = 0;
#if !defined(_WIN32) && defined(TIOCGWINSZ)
    winsize ws{};
    if (::ioctl(fd, TIOCGWINSZ, &ws) == 0) {
        w = ws.ws_col;
        h = ws.ws_row;
    }
#else
    (void)fd;
#endif
    if (const char* s = std::getenv("COLUMNS")) {
        unsigned v = 0;
        auto sv = std::string_view{s};
        if (std::from_chars(sv.data(), sv.data() + sv.size(), v).ec == std::errc{} && v > 0 && !w) {
            w = v;
        }
    }
    if (const char* s = std::getenv("LINES")) {
        unsigned v = 0;
        auto sv = std::string_view{s};
        if (std::from_chars(sv.data(), sv.data() + sv.size(), v).ec == std::errc{} && v > 0 && !h) {
            h = v;
        }
    }
    if (!w) w = kDefaultScreenW;
    if (!h) h = kDefaultScreenH;
    return true;
}

// Result of one round of polling stdin.
enum class LineState {
    Timeout,     // no input this tick
    Continue,    // got chars, line not yet complete
    Ready,       // got Enter -- caller should send `buffer`
    Cancelled,   // got ESC -- abort
};

#ifndef _WIN32

// Configure stdin for non-canonical, no-echo, short-timeout reads.
// Returns the previous attributes (to restore on exit).  On failure
// returns std::nullopt.
struct TtyGuard {
    int            fd{-1};
    termios        saved{};
    bool           active{false};

    bool engage(int fd_in) noexcept {
        fd = fd_in;
        if (::tcgetattr(fd, &saved) < 0) {
            return false;
        }
        termios mod = saved;
        mod.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON);
        mod.c_cc[VMIN]  = 0;
        mod.c_cc[VTIME] = 1; // 0.1s
        if (::tcsetattr(fd, TCSANOW, &mod) < 0) {
            return false;
        }
        active = true;
        return true;
    }
    ~TtyGuard() {
        if (active) {
            ::tcsetattr(fd, TCSAFLUSH, &saved);
        }
    }
};

LineState poll_keyboard(int fd, std::string& buffer) noexcept {
    LineState st = LineState::Timeout;
    for (int i = 0; i < 16; ++i) {
        char c = 0;
        auto n = ::read(fd, &c, 1);
        if (n <= 0) {
            return st;
        }
        st = LineState::Continue;
        switch (c) {
            case '\033':                     // ESC
                return LineState::Cancelled;
            case '\r':
            case '\n':
                return LineState::Ready;
            case '\b':
            case '\177':                     // BS / DEL
                if (!buffer.empty()) {
                    buffer.pop_back();
                }
                continue;
            default:
                if (buffer.size() + 1 < kMaxLineLen) {
                    buffer.push_back(c);
                }
        }
    }
    return st;
}

#else // _WIN32

struct TtyGuard {
    bool engage(int /*fd*/) noexcept { return true; }
    ~TtyGuard() = default;
};

LineState poll_keyboard(int /*fd*/, std::string& buffer) noexcept {
    LineState st = LineState::Timeout;
    for (int i = 0; i < 16; ++i) {
        if (!_kbhit()) {
            return st;
        }
        int c = _getch();
        st = LineState::Continue;
        switch (c) {
            case 0x1b:
                return LineState::Cancelled;
            case '\r':
            case '\n':
                return LineState::Ready;
            case '\b':
            case 0x7f:
                if (!buffer.empty()) {
                    buffer.pop_back();
                }
                continue;
            default:
                if (buffer.size() + 1 < kMaxLineLen) {
                    buffer.push_back(static_cast<char>(c));
                }
        }
    }
    return st;
}

#endif

int run(int argc, char** argv) {
    std::string_view servname;
    std::uint16_t    servport = 0;

    for (int a = 1; a < argc; ++a) {
        std::string_view arg{argv[a]};
        if (arg == "-h" || arg == "--help" || arg == "--usage") {
            usage(argv[0]);
        } else if (arg == "-v" || arg == "--version") {
            std::println("version {}", PVPGN_VERSION);
            return EXIT_SUCCESS;
        } else if (!arg.empty() && arg.front() == '-') {
            std::println(stderr, "{}: unknown option \"{}\"", argv[0], arg);
            usage(argv[0]);
        } else if (servname.empty()) {
            servname = arg;
        } else if (servport == 0) {
            if (!parse_port(arg, servport)) {
                std::println(stderr, "{}: \"{}\" should be a positive integer", argv[0], arg);
                usage(argv[0]);
            }
        } else {
            std::println(stderr, "{}: too many arguments", argv[0]);
            usage(argv[0]);
        }
    }

    if (servport == 0)      servport = kDefaultPort;
    if (servname.empty())   servname = kDefaultHost;

    default_logger().set_level(LogLevel::Info);

    if (!pvpgn::client_v3::net::sockets_startup()) {
        std::println(stderr, "{}: could not initialize socket subsystem", argv[0]);
        return EXIT_FAILURE;
    }

    socket_holder sock{pvpgn::client_v3::net::connect_tcp(servname, servport)};
    if (!sock.valid()) {
        std::println(stderr, "{}: could not connect to server \"{}\" port {}", argv[0], servname, servport);
        pvpgn::client_v3::net::sockets_cleanup();
        return EXIT_FAILURE;
    }

    {
        sockaddr_in resolved{};
        pvpgn::client_v3::net::resolve_host(servname, servport, resolved);
        std::println("Connected to {}:{}.",
                    pvpgn::client_v3::net::addr_to_string(resolved).c_str(),
                    servport);
    }

    const int fd_stdin = fileno(stdin);

    TtyGuard tty;
    if (!tty.engage(fd_stdin)) {
        std::println(stderr, "{}: could not set terminal attributes for stdin",
                     argv[0]);
    }

    unsigned screen_w = 0, screen_h = 0;
    if (!query_term_size(fd_stdin, screen_w, screen_h)) {
        std::println(stderr, "{}: could not determine screen size", argv[0]);
        pvpgn::client_v3::net::sockets_cleanup();
        return EXIT_FAILURE;
    }
    (void)screen_w;
    (void)screen_h;

    using ::pvpgn::client_v3::proto::send_init_classbyte;
    using ::pvpgn::client_v3::net::send_all;
    using ::pvpgn::client_v3::net::recv_all;

    if (!send_init_classbyte(sock.get(), kInitBot)) {
        std::println(stderr, "{}: could not send init class byte", argv[0]);
        pvpgn::client_v3::net::sockets_cleanup();
        return EXIT_FAILURE;
    }
    {
        const char marker[2] = { kBotProtocolMarker, '\0' };
        if (!send_all(sock.get(), marker, sizeof(marker))) {
            std::println(stderr, "{}: could not send bot marker", argv[0]);
            pvpgn::client_v3::net::sockets_cleanup();
            return EXIT_FAILURE;
        }
    }

    std::string         line;
    std::array<char, 4096> netbuf{};

    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
#ifdef _WIN32
        // Windows select() on stdin is unreliable; fall back to a
        // small timeout and rely on _kbhit() for keyboard polling.
        FD_SET(sock.get(), &rfds);
        timeval tv{};
        tv.tv_sec  = 0;
        tv.tv_usec = 100000; // 0.1s
        int rc = ::select(static_cast<int>(sock.get()) + 1, &rfds, nullptr, nullptr, &tv);
        if (rc < 0) {
            int err = pvpgn::client_v3::net::last_error();
            if (err == EINTR) continue;
            std::println(stderr, "{}: select failed ({})", argv[0], err);
            break;
        }
        const bool sock_ready = rc > 0 && FD_ISSET(sock.get(), &rfds);
        const bool stdin_ready = true; // poll_keyboard handles "nothing pending"
#else
        FD_SET(fd_stdin, &rfds);
        FD_SET(sock.get(), &rfds);
        const int nfds = std::max(fd_stdin, sock.get()) + 1;
        int rc = ::select(nfds, &rfds, nullptr, nullptr, nullptr);
        if (rc < 0) {
            if (errno == EINTR) continue;
            std::println(stderr, "{}: select failed ({})", argv[0], std::strerror(errno));
            continue;
        }
        const bool sock_ready  = FD_ISSET(static_cast<std::size_t>(sock.get()),  &rfds);
        const bool stdin_ready = FD_ISSET(static_cast<std::size_t>(fd_stdin),    &rfds);
#endif

        if (sock_ready) {
            auto n = ::recv(sock.get(), netbuf.data(),
                            static_cast<int>(netbuf.size() - 1), 0);
            if (n <= 0) {
                std::println("Connection closed by server.");
                pvpgn::client_v3::net::sockets_cleanup();
                return EXIT_SUCCESS;
            }
            netbuf[static_cast<std::size_t>(n)] = '\0';
            std::fputs(netbuf.data(), stdout);
            std::fflush(stdout);
        }

        if (stdin_ready) {
            switch (poll_keyboard(fd_stdin, line)) {
                case LineState::Cancelled:
                    pvpgn::client_v3::net::sockets_cleanup();
                    return EXIT_FAILURE;
                case LineState::Timeout:
                case LineState::Continue:
                    break;
                case LineState::Ready: {
                    line.append("\r\n");
                    if (!send_all(sock.get(), line.data(), line.size())) {
                        std::println(stderr, "{}: send failed", argv[0]);
                        pvpgn::client_v3::net::sockets_cleanup();
                        return EXIT_FAILURE;
                    }
                    line.clear();
                    break;
                }
            }
        }
    }

    pvpgn::client_v3::net::sockets_cleanup();
    return EXIT_SUCCESS;
}

} // namespace

extern int main(int argc, char* argv[]);
int main(int argc, char* argv[]) {
    return run(argc, argv);
}
