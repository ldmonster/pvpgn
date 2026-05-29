// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/tracker/tracker_client.hpp"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <vector>

namespace pvpgn::infra::tracker {

namespace {

void write_be16(std::byte* dst, std::uint16_t v) {
    dst[0] = static_cast<std::byte>((v >> 8) & 0xFF);
    dst[1] = static_cast<std::byte>(v & 0xFF);
}

void write_be32(std::byte* dst, std::uint32_t v) {
    dst[0] = static_cast<std::byte>((v >> 24) & 0xFF);
    dst[1] = static_cast<std::byte>((v >> 16) & 0xFF);
    dst[2] = static_cast<std::byte>((v >> 8) & 0xFF);
    dst[3] = static_cast<std::byte>(v & 0xFF);
}

void write_cstr(std::byte* dst, std::size_t cap, std::string_view src) {
    std::size_t n = std::min(cap == 0 ? 0 : cap - 1, src.size());
    for (std::size_t i = 0; i < n; ++i) {
        dst[i] = static_cast<std::byte>(static_cast<unsigned char>(src[i]));
    }
    // Zero-pad the remainder (including the terminator slot).
    for (std::size_t i = n; i < cap; ++i) {
        dst[i] = std::byte{0};
    }
}

}  // namespace

std::vector<std::byte>
encode_trackpacket(std::uint16_t listen_port, const ReportStats& stats) {
    std::vector<std::byte> buf(kTrackPacketSize, std::byte{0});
    std::byte* p = buf.data();

    write_be16(p +   0, kTrackVersion);
    write_be16(p +   2, listen_port);
    write_be32(p +   4, stats.flags);
    write_cstr(p +   8, 32, stats.software);
    write_cstr(p +  40, 16, stats.version);
    write_cstr(p +  56, 32, stats.platform);
    write_cstr(p +  88, 64, stats.server_desc);
    write_cstr(p + 152, 64, stats.server_location);
    write_cstr(p + 216, 96, stats.server_url);
    write_cstr(p + 312, 64, stats.contact_name);
    write_cstr(p + 376, 64, stats.contact_email);
    write_be32(p + 440, stats.users);
    write_be32(p + 444, stats.channels);
    write_be32(p + 448, stats.games);
    write_be32(p + 452, stats.uptime);
    write_be32(p + 456, stats.total_games);
    write_be32(p + 460, stats.total_logins);

    return buf;
}

std::vector<TrackServer>
parse_servers(std::string_view csv, std::uint16_t default_port) {
    std::vector<TrackServer> out;
    std::size_t              i = 0;
    while (i < csv.size()) {
        std::size_t comma = csv.find(',', i);
        std::string_view seg = csv.substr(i, comma - i);
        i = (comma == std::string_view::npos) ? csv.size() : comma + 1;

        // Trim leading/trailing whitespace.
        while (!seg.empty() && (seg.front() == ' ' || seg.front() == '\t')) {
            seg.remove_prefix(1);
        }
        while (!seg.empty() && (seg.back() == ' ' || seg.back() == '\t')) {
            seg.remove_suffix(1);
        }
        if (seg.empty()) continue;

        TrackServer ts{};
        ts.port = default_port;

        std::size_t colon = seg.rfind(':');
        if (colon == std::string_view::npos) {
            ts.host.assign(seg.data(), seg.size());
        } else {
            std::string_view host_sv = seg.substr(0, colon);
            std::string_view port_sv = seg.substr(colon + 1);
            if (host_sv.empty() || port_sv.empty()) continue;
            std::uint16_t port = 0;
            auto res = std::from_chars(port_sv.data(),
                                       port_sv.data() + port_sv.size(),
                                       port);
            if (res.ec != std::errc{} || res.ptr != port_sv.data() + port_sv.size()) {
                continue;
            }
            ts.host.assign(host_sv.data(), host_sv.size());
            ts.port = port;
        }

        out.push_back(std::move(ts));
    }
    return out;
}

}  // namespace pvpgn::infra::tracker
