// SPDX-License-Identifier: GPL-2.0-or-later
// bntrackd_types.hpp — Shared types, constants and globals for bntrackd.
//
// Included inside bntrackd.cpp's anonymous namespace (after all system
// headers have been included at file scope).  Not a standalone TU.

// Platform socket typedefs — defined here so sub-TUs can reference them.
// The actual system headers are included at file scope in bntrackd.cpp.
#ifdef _WIN32
   using socket_t          = SOCKET;
   using socklen_portable  = int;
   static constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#  define BNTRACKD_CLOSESOCKET closesocket
#else
   using socket_t          = int;
   using socklen_portable  = socklen_t;
   static constexpr socket_t kInvalidSocket = -1;
#  define BNTRACKD_CLOSESOCKET ::close
#  define DO_DAEMONIZE 1
#endif

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

// Globals — defined once in bntrackd.cpp's anonymous namespace.
// All sub-TUs share them via the enclosing anonymous namespace.
Prefs                    g_prefs;
std::vector<ServerEntry> g_servers;
