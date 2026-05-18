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
   using socket_t          = SOCKET;
   using socklen_portable  = int;
   static constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#  define BNTRACKD_CLOSESOCKET closesocket
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <sys/types.h>
#  include <unistd.h>
   using socket_t          = int;
   using socklen_portable  = socklen_t;
   static constexpr socket_t kInvalidSocket = -1;
#  define BNTRACKD_CLOSESOCKET ::close
#  define DO_DAEMONIZE 1
#endif

#ifndef PVPGN_VERSION
#define PVPGN_VERSION "unknown"
#endif

namespace {

constexpr std::uint16_t kTrackVersion        = 2;
constexpr std::uint32_t kFlagShutdown        = 0x1;
constexpr int           kGranularitySeconds  = 1;
constexpr unsigned      kDefaultPort         = 6114;
constexpr unsigned      kDefaultUpdate       = 60;
constexpr unsigned      kDefaultExpire       = 600;
constexpr const char *  kDefaultOutfile      = "bnetd-servers";
constexpr const char *  kDefaultPidfile      = "";
constexpr const char *  kDefaultLogfile      = "";

// Wire layout of the tracker UDP packet. Strings are NUL-terminated
// inside their fixed-size slots; multi-byte integers arrive in
// network byte order.
struct TrackPacket {
    std::uint16_t packet_version;
    std::uint16_t port;
    std::uint32_t flags;
    std::array<char, 32> software{};
    std::array<char, 16> version{};
    std::array<char, 32> platform{};
    std::array<char, 64> server_desc{};
    std::array<char, 64> server_location{};
    std::array<char, 96> server_url{};
    std::array<char, 64> contact_name{};
    std::array<char, 64> contact_email{};
    std::uint32_t users;
    std::uint32_t channels;
    std::uint32_t games;
    std::uint32_t uptime;
    std::uint32_t total_games;
    std::uint32_t total_logins;
};

constexpr std::size_t kPacketWireSize =
    2 + 2 + 4 + 32 + 16 + 32 + 64 + 64 + 96 + 64 + 64 + 4 * 6;
static_assert(kPacketWireSize == 464,
    "tracker wire packet must be 464 bytes");

struct ServerEntry {
    in_addr     address{};
    std::time_t updated = 0;
    TrackPacket info{};
};

struct Prefs {
    int            foreground = 0;
    int            debug      = 0;
    int            xml_mode   = 0;
    unsigned int   expire     = 0;
    unsigned int   update     = 0;
    std::uint16_t  port       = 0;
    const char *   outfile    = nullptr;
    const char *   pidfile    = nullptr;
    const char *   logfile    = nullptr;
    const char *   process    = nullptr;
};

Prefs                    g_prefs;
std::vector<ServerEntry> g_servers;

// ---------------------------------------------------------------------------
// Wire helpers
// ---------------------------------------------------------------------------

bool decode_packet(const unsigned char *buf, std::size_t len,
                   TrackPacket &out)
{
    if (len < kPacketWireSize) return false;

    auto read_u16 = [&buf](std::size_t off) -> std::uint16_t {
        std::uint16_t v = 0;
        std::memcpy(&v, buf + off, sizeof(v));
        return ntohs(v);
    };
    auto read_u32 = [&buf](std::size_t off) -> std::uint32_t {
        std::uint32_t v = 0;
        std::memcpy(&v, buf + off, sizeof(v));
        return ntohl(v);
    };
    auto read_str = [&buf](std::size_t off, char *dst, std::size_t n) {
        std::memcpy(dst, buf + off, n);
        dst[n - 1] = '\0';
    };

    std::size_t off = 0;
    out.packet_version = read_u16(off); off += 2;
    out.port           = read_u16(off); off += 2;
    out.flags          = read_u32(off); off += 4;
    read_str(off, out.software.data(),        out.software.size());        off += out.software.size();
    read_str(off, out.version.data(),         out.version.size());         off += out.version.size();
    read_str(off, out.platform.data(),        out.platform.size());        off += out.platform.size();
    read_str(off, out.server_desc.data(),     out.server_desc.size());     off += out.server_desc.size();
    read_str(off, out.server_location.data(), out.server_location.size()); off += out.server_location.size();
    read_str(off, out.server_url.data(),      out.server_url.size());      off += out.server_url.size();
    read_str(off, out.contact_name.data(),    out.contact_name.size());    off += out.contact_name.size();
    read_str(off, out.contact_email.data(),   out.contact_email.size());   off += out.contact_email.size();
    out.users        = read_u32(off); off += 4;
    out.channels     = read_u32(off); off += 4;
    out.games        = read_u32(off); off += 4;
    out.uptime       = read_u32(off); off += 4;
    out.total_games  = read_u32(off); off += 4;
    out.total_logins = read_u32(off); off += 4;
    return true;
}

// Defang `##` runs inside packet strings: the legacy ASCII output
// format uses `##` as a record separator, so server-supplied strings
// must not contain it. Replace the second `#` with `%` to match the
// legacy daemon's `fixup_str()`.
void fixup_str(char *s, std::size_t n)
{
    char prev = '\0';
    for (std::size_t i = 0; i < n && s[i] != '\0'; ++i) {
        if (prev == '#' && s[i] == '#') s[i] = '%';
        prev = s[i];
    }
}

// ---------------------------------------------------------------------------
// Option parsing
// ---------------------------------------------------------------------------

[[noreturn]] void usage(const char *progname);

bool parse_uint(std::string_view sv, unsigned int &out)
{
    unsigned int v = 0;
    const char *first = sv.data();
    const char *last  = sv.data() + sv.size();
    auto [p, ec] = std::from_chars(first, last, v);
    if (ec != std::errc{} || p != last) return false;
    out = v;
    return true;
}

bool parse_ushort(std::string_view sv, std::uint16_t &out)
{
    unsigned int v = 0;
    if (!parse_uint(sv, v)) return false;
    if (v > 0xFFFFu) return false;
    out = static_cast<std::uint16_t>(v);
    return true;
}

void getprefs(int argc, char *argv[])
{
    g_prefs = Prefs{};

    auto need_arg = [&](int &a) {
        if (a + 1 >= argc) {
            std::fprintf(stderr,
                "%s: option \"%s\" requires an argument\n",
                argv[0], argv[a]);
            usage(argv[0]);
        }
        ++a;
        return argv[a];
    };

    for (int a = 1; a < argc; ++a) {
        std::string_view arg = argv[a];

        if (arg.starts_with("--command=")) {
            if (g_prefs.process) {
                std::fprintf(stderr,
                    "%s: processing command was already specified as \"%s\"\n",
                    argv[0], g_prefs.process);
                usage(argv[0]);
            }
            g_prefs.process = argv[a] + 10;
        }
        else if (arg == "-c") {
            const char *val = need_arg(a);
            if (g_prefs.process) {
                std::fprintf(stderr,
                    "%s: processing command was already specified as \"%s\"\n",
                    argv[0], g_prefs.process);
                usage(argv[0]);
            }
            g_prefs.process = val;
        }
        else if (arg == "-d" || arg == "--debug") {
            g_prefs.debug = 1;
        }
        else if (arg.starts_with("--expire=")) {
            if (g_prefs.expire) {
                std::fprintf(stderr,
                    "%s: expiration period was already specified as \"%u\"\n",
                    argv[0], g_prefs.expire);
                usage(argv[0]);
            }
            if (!parse_uint(arg.substr(9), g_prefs.expire)) {
                std::fprintf(stderr,
                    "%s: \"%s\" should be a positive integer\n",
                    argv[0], argv[a] + 9);
                usage(argv[0]);
            }
        }
        else if (arg == "-e") {
            const char *val = need_arg(a);
            if (g_prefs.expire) {
                std::fprintf(stderr,
                    "%s: expiration period was already specified as \"%u\"\n",
                    argv[0], g_prefs.expire);
                usage(argv[0]);
            }
            if (!parse_uint(val, g_prefs.expire)) {
                std::fprintf(stderr,
                    "%s: \"%s\" should be a positive integer\n",
                    argv[0], val);
                usage(argv[0]);
            }
        }
        else if (arg == "-f" || arg == "--foreground") {
            g_prefs.foreground = 1;
        }
        else if (arg == "-x" || arg == "--XML") {
            g_prefs.xml_mode = 1;
        }
        else if (arg.starts_with("--logfile=")) {
            if (g_prefs.logfile) {
                std::fprintf(stderr,
                    "%s: eventlog file was already specified as \"%s\"\n",
                    argv[0], g_prefs.logfile);
                usage(argv[0]);
            }
            g_prefs.logfile = argv[a] + 10;
        }
        else if (arg == "-l") {
            const char *val = need_arg(a);
            if (g_prefs.logfile) {
                std::fprintf(stderr,
                    "%s: eventlog file was already specified as \"%s\"\n",
                    argv[0], g_prefs.logfile);
                usage(argv[0]);
            }
            g_prefs.logfile = val;
        }
        else if (arg.starts_with("--outfile=")) {
            if (g_prefs.outfile) {
                std::fprintf(stderr,
                    "%s: output file was already specified as \"%s\"\n",
                    argv[0], g_prefs.outfile);
                usage(argv[0]);
            }
            g_prefs.outfile = argv[a] + 10;
        }
        else if (arg == "-o") {
            const char *val = need_arg(a);
            if (g_prefs.outfile) {
                std::fprintf(stderr,
                    "%s: output file was already specified as \"%s\"\n",
                    argv[0], g_prefs.outfile);
                usage(argv[0]);
            }
            g_prefs.outfile = val;
        }
        else if (arg.starts_with("--pidfile=")) {
            if (g_prefs.pidfile) {
                std::fprintf(stderr,
                    "%s: pid file was already specified as \"%s\"\n",
                    argv[0], g_prefs.pidfile);
                usage(argv[0]);
            }
            g_prefs.pidfile = argv[a] + 10;
        }
        else if (arg == "-P") {
            const char *val = need_arg(a);
            if (g_prefs.pidfile) {
                std::fprintf(stderr,
                    "%s: pid file was already specified as \"%s\"\n",
                    argv[0], g_prefs.pidfile);
                usage(argv[0]);
            }
            g_prefs.pidfile = val;
        }
        else if (arg.starts_with("--port=")) {
            if (g_prefs.port) {
                std::fprintf(stderr,
                    "%s: port number was already specified as \"%hu\"\n",
                    argv[0], g_prefs.port);
                usage(argv[0]);
            }
            if (!parse_ushort(arg.substr(7), g_prefs.port)) {
                std::fprintf(stderr,
                    "%s: \"%s\" should be a positive integer\n",
                    argv[0], argv[a] + 7);
                usage(argv[0]);
            }
        }
        else if (arg == "-p") {
            const char *val = need_arg(a);
            if (g_prefs.port) {
                std::fprintf(stderr,
                    "%s: port number was already specified as \"%hu\"\n",
                    argv[0], g_prefs.port);
                usage(argv[0]);
            }
            if (!parse_ushort(val, g_prefs.port)) {
                std::fprintf(stderr,
                    "%s: \"%s\" should be a positive integer\n",
                    argv[0], val);
                usage(argv[0]);
            }
        }
        else if (arg.starts_with("--update=")) {
            if (g_prefs.update) {
                std::fprintf(stderr,
                    "%s: update period was already specified as \"%u\"\n",
                    argv[0], g_prefs.update);
                usage(argv[0]);
            }
            if (!parse_uint(arg.substr(9), g_prefs.update)) {
                std::fprintf(stderr,
                    "%s: \"%s\" should be a positive integer\n",
                    argv[0], argv[a] + 9);
                usage(argv[0]);
            }
        }
        else if (arg == "-u") {
            const char *val = need_arg(a);
            if (g_prefs.update) {
                std::fprintf(stderr,
                    "%s: update period was already specified as \"%u\"\n",
                    argv[0], g_prefs.update);
                usage(argv[0]);
            }
            if (!parse_uint(val, g_prefs.update)) {
                std::fprintf(stderr,
                    "%s: \"%s\" should be a positive integer\n",
                    argv[0], val);
                usage(argv[0]);
            }
        }
        else if (arg == "-h" || arg == "--help" || arg == "--usage") {
            usage(argv[0]);
        }
        else if (arg == "-v" || arg == "--version") {
            std::printf("bntrackd version " PVPGN_VERSION "\n");
            std::exit(EXIT_SUCCESS);
        }
        else {
            std::fprintf(stderr, "%s: unrecognized option \"%s\"\n",
                argv[0], argv[a]);
            usage(argv[0]);
        }
    }

    if (!g_prefs.process) g_prefs.process = "";
    if (g_prefs.update == 0) g_prefs.update = kDefaultUpdate;
    if (g_prefs.expire == 0) g_prefs.expire = kDefaultExpire;
    if (!g_prefs.logfile)    g_prefs.logfile = kDefaultLogfile;
    if (!g_prefs.outfile)    g_prefs.outfile = kDefaultOutfile;
    if (g_prefs.port == 0)
        g_prefs.port = static_cast<std::uint16_t>(kDefaultPort);
    if (!g_prefs.pidfile)    g_prefs.pidfile = kDefaultPidfile;

    if (g_prefs.logfile && g_prefs.logfile[0] == '\0')
        g_prefs.logfile = nullptr;
    if (g_prefs.pidfile && g_prefs.pidfile[0] == '\0')
        g_prefs.pidfile = nullptr;
}

[[noreturn]] void usage(const char *progname)
{
    std::fprintf(stderr, "usage: %s [<options>]\n", progname);
    std::fprintf(stderr,
        "  -c COMMAND, --command=COMMAND  execute COMMAND update\n"
        "  -d, --debug                    turn on debug mode\n"
        "  -e SECS, --expire SECS         forget a list entry after SEC seconds\n"
#ifdef DO_DAEMONIZE
        "  -f, --foreground               don't daemonize\n"
#else
        "  -f, --foreground               don't daemonize (default)\n"
#endif
        "  -l FILE, --logfile=FILE        write event messages to FILE\n"
        "  -o FILE, --outfile=FILE        write server list to FILE\n");
    std::fprintf(stderr,
        "  -p PORT, --port=PORT           listen for announcments on UDP port PORT\n"
        "  -P FILE, --pidfile=FILE        write pid to FILE\n"
        "  -u SECS, --update SECS         write output file every SEC seconds\n"
        "  -x, --XML                      write output file in XML format\n"
        "  -h, --help, --usage            show this information and exit\n"
        "  -v, --version                  print version number and exit\n");
    std::exit(EXIT_FAILURE);
}

// ---------------------------------------------------------------------------
// Output file writer
// ---------------------------------------------------------------------------

void write_outfile(std::FILE *outfile, std::time_t now)
{
    std::erase_if(g_servers, [&](const ServerEntry &e) {
        return e.updated +
               static_cast<std::time_t>(g_prefs.expire) < now;
    });

    for (const auto &server : g_servers) {
        char addrstr[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &server.address, addrstr, sizeof(addrstr));

        const TrackPacket &p = server.info;
        if (g_prefs.xml_mode == 1) {
            std::fprintf(outfile, "<server>\n\t<address>%s</address>\n", addrstr);
            std::fprintf(outfile, "\t<port>%" PRIu16 "</port>\n",                p.port);
            std::fprintf(outfile, "\t<location>%s</location>\n",                 p.server_location.data());
            std::fprintf(outfile, "\t<software>%s</software>\n",                 p.software.data());
            std::fprintf(outfile, "\t<version>%s</version>\n",                   p.version.data());
            std::fprintf(outfile, "\t<users>%" PRIu32 "</users>\n",              p.users);
            std::fprintf(outfile, "\t<channels>%" PRIu32 "</channels>\n",        p.channels);
            std::fprintf(outfile, "\t<games>%" PRIu32 "</games>\n",              p.games);
            std::fprintf(outfile, "\t<description>%s</description>\n",           p.server_desc.data());
            std::fprintf(outfile, "\t<platform>%s</platform>\n",                 p.platform.data());
            std::fprintf(outfile, "\t<url>%s</url>\n",                           p.server_url.data());
            std::fprintf(outfile, "\t<contact_name>%s</contact_name>\n",         p.contact_name.data());
            std::fprintf(outfile, "\t<contact_email>%s</contact_email>\n",       p.contact_email.data());
            std::fprintf(outfile, "\t<uptime>%" PRIu32 "</uptime>\n",            p.uptime);
            std::fprintf(outfile, "\t<total_games>%" PRIu32 "</total_games>\n",  p.total_games);
            std::fprintf(outfile, "\t<logins>%" PRIu32 "</logins>\n",            p.total_logins);
            std::fprintf(outfile, "</server>\n");
        }
        else {
            std::fprintf(outfile, "%s\n##\n", addrstr);
            std::fprintf(outfile, "%" PRIu16 "\n##\n", p.port);
            std::fprintf(outfile, "%s\n##\n",          p.server_location.data());
            std::fprintf(outfile, "%s\n##\n",          p.software.data());
            std::fprintf(outfile, "%s\n##\n",          p.version.data());
            std::fprintf(outfile, "%" PRIu32 "\n##\n", p.users);
            std::fprintf(outfile, "%" PRIu32 "\n##\n", p.channels);
            std::fprintf(outfile, "%" PRIu32 "\n##\n", p.games);
            std::fprintf(outfile, "%s\n##\n",          p.server_desc.data());
            std::fprintf(outfile, "%s\n##\n",          p.platform.data());
            std::fprintf(outfile, "%s\n##\n",          p.server_url.data());
            std::fprintf(outfile, "%s\n##\n",          p.contact_name.data());
            std::fprintf(outfile, "%s\n##\n",          p.contact_email.data());
            std::fprintf(outfile, "%" PRIu32 "\n##\n", p.uptime);
            std::fprintf(outfile, "%" PRIu32 "\n##\n", p.total_games);
            std::fprintf(outfile, "%" PRIu32 "\n##\n", p.total_logins);
            std::fprintf(outfile, "###\n");
        }
    }
}

// ---------------------------------------------------------------------------
// Main socket loop
// ---------------------------------------------------------------------------

int server_process(socket_t sockfd)
{
    std::time_t last = std::time(nullptr) - g_prefs.update;

    for (;;) {
        const std::time_t now = std::time(nullptr);

        if (last + static_cast<std::time_t>(g_prefs.update) < now) {
            last = now;
            std::FILE *outfile = std::fopen(g_prefs.outfile, "w");
            if (!outfile) {
                LOG_ERROR("bntrackd",
                    "unable to open file \"{}\" for writing (fopen: {})",
                    g_prefs.outfile, std::strerror(errno));
            }
            else {
                write_outfile(outfile, last);
                if (std::fclose(outfile) < 0) {
                    LOG_ERROR("bntrackd",
                        "could not close output file \"{}\" after writing"
                        " (fclose: {})",
                        g_prefs.outfile, std::strerror(errno));
                }
                if (g_prefs.process && g_prefs.process[0] != '\0') {
                    std::system(g_prefs.process);
                }
            }
        }

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sockfd, &rfds);
        timeval tv{};
        tv.tv_sec  = kGranularitySeconds;
        tv.tv_usec = 0;
        const int s = select(static_cast<int>(sockfd) + 1, &rfds, nullptr,
            nullptr, &tv);
        if (s < 0) {
#ifdef EINTR
            if (errno == EINTR) continue;
#endif
            LOG_ERROR("bntrackd", "select failed (select: {})",
                std::strerror(errno));
            continue;
        }
        if (s == 0) continue;
        if (!FD_ISSET(sockfd, &rfds)) continue;

        unsigned char buf[1024]{};
        sockaddr_in cliaddr{};
        socklen_portable clilen = sizeof(cliaddr);
        const auto n = recvfrom(sockfd,
            reinterpret_cast<char*>(buf), sizeof(buf), 0,
            reinterpret_cast<sockaddr*>(&cliaddr), &clilen);
        if (n < 0) continue;

        TrackPacket packet{};
        if (!decode_packet(buf, static_cast<std::size_t>(n), packet)) continue;
        if (packet.packet_version < kTrackVersion) continue;

        fixup_str(packet.software.data(),        packet.software.size());
        fixup_str(packet.version.data(),         packet.version.size());
        fixup_str(packet.platform.data(),        packet.platform.size());
        fixup_str(packet.server_desc.data(),     packet.server_desc.size());
        fixup_str(packet.server_location.data(), packet.server_location.size());
        fixup_str(packet.server_url.data(),      packet.server_url.size());
        fixup_str(packet.contact_name.data(),    packet.contact_name.size());
        fixup_str(packet.contact_email.data(),   packet.contact_email.size());

        auto it = std::find_if(g_servers.begin(), g_servers.end(),
            [&](const ServerEntry &e) {
                return std::memcmp(&e.address, &cliaddr.sin_addr,
                    sizeof(in_addr)) == 0;
            });

        if (it != g_servers.end()) {
            if (packet.flags & kFlagShutdown) {
                g_servers.erase(it);
            }
            else {
                it->info    = packet;
                it->updated = std::time(nullptr);
            }
        }
        else if (!(packet.flags & kFlagShutdown)) {
            ServerEntry entry{};
            entry.address = cliaddr.sin_addr;
            entry.info    = packet;
            entry.updated = std::time(nullptr);
            g_servers.push_back(entry);
        }

        char addrstr[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &cliaddr.sin_addr, addrstr, sizeof(addrstr));

        LOG_DEBUG("bntrackd",
            "Packet received from {}:"
            " packet_version={}"
            " flags=0x{:08x}"
            " port={}"
            " software=\"{}\""
            " version=\"{}\""
            " platform=\"{}\""
            " server_desc=\"{}\""
            " server_location=\"{}\""
            " server_url=\"{}\""
            " contact_name=\"{}\""
            " contact_email=\"{}\""
            " uptime={}"
            " total_games={}"
            " total_logins={}",
            addrstr,
            packet.packet_version,
            packet.flags,
            packet.port,
            packet.software.data(),
            packet.version.data(),
            packet.platform.data(),
            packet.server_desc.data(),
            packet.server_location.data(),
            packet.server_url.data(),
            packet.contact_name.data(),
            packet.contact_email.data(),
            packet.uptime,
            packet.total_games,
            packet.total_logins);
    }
}

// ---------------------------------------------------------------------------
// Networking init
// ---------------------------------------------------------------------------

#ifdef _WIN32
class WsaInit {
public:
    WsaInit() {
        WSADATA wsa{};
        ok_ = (WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
    }
    ~WsaInit() { if (ok_) WSACleanup(); }
    [[nodiscard]] bool ok() const noexcept { return ok_; }
private:
    bool ok_ = false;
};
#endif

unsigned long current_pid()
{
#ifdef _WIN32
    return static_cast<unsigned long>(_getpid());
#else
    return static_cast<unsigned long>(::getpid());
#endif
}

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
