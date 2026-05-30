// SPDX-License-Identifier: GPL-2.0-or-later
// bntrackd_protocol.cpp — UDP wire-packet decode + string sanitisation.
//
// Included into bntrackd.cpp's anonymous namespace via #include.
// Not a standalone compilation unit.

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
