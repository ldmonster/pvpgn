// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file tracker_client.hpp
/// UDP client that periodically reports server statistics to one or more
/// pvpgn "track" servers, wire-compatible with the legacy
/// `t_trackpacket` (see `src/common/tracker.h`, TRACK_VERSION=2).

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/bytes.hpp"
#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::infra::tracker {

/// Wire-format constants -- must match legacy.
inline constexpr std::uint16_t kTrackVersion       = 2;
inline constexpr std::uint16_t kTrackDefaultPort   = 6114;  ///< BNETD_TRACK_PORT
inline constexpr std::size_t   kTrackPacketSize    = 464;

inline constexpr std::uint32_t kTrackFlagShutdown  = 0x1;
inline constexpr std::uint32_t kTrackFlagPrivate   = 0x2;

/// Server identity + counters reported to each track server.
struct ReportStats {
    std::string   software;       ///< NUL-terminated, fits 32 bytes
    std::string   version;        ///< fits 16
    std::string   platform;       ///< fits 32 (e.g. uname().sysname)
    std::string   server_desc;    ///< fits 64
    std::string   server_location;///< fits 64
    std::string   server_url;     ///< fits 96
    std::string   contact_name;   ///< fits 64
    std::string   contact_email;  ///< fits 64

    std::uint32_t users         = 0;
    std::uint32_t channels      = 0;
    std::uint32_t games         = 0;
    std::uint32_t uptime        = 0;
    std::uint32_t total_games   = 0;
    std::uint32_t total_logins  = 0;

    std::uint32_t flags         = 0;
};

/// Parsed track-server endpoint.
struct TrackServer {
    std::string   host;   ///< hostname or dotted IPv4
    std::uint16_t port;
};

/// Encode a `t_trackpacket` for the given listen port and stats.
/// Result is always exactly `kTrackPacketSize` bytes, big-endian for
/// every multi-byte field; trailing slots are zero-padded.
std::vector<std::byte>
encode_trackpacket(std::uint16_t listen_port, const ReportStats& stats);

/// Parse "host[:port][,host[:port]...]" into a list of TrackServer.
/// Uses `default_port` when a port suffix is absent.
/// Returns the (possibly empty) parsed list; malformed segments are
/// skipped, never produce an error.
std::vector<TrackServer>
parse_servers(std::string_view csv,
              std::uint16_t    default_port = kTrackDefaultPort);

}  // namespace pvpgn::infra::tracker
