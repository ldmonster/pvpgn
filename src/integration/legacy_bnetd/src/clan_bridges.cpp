// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/clan_bridges.hpp"

#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

namespace {

bool finalize_into(pvpgn::protocol::Writer& w,
                   unsigned char*           out_buf,
                   unsigned int             max_size,
                   unsigned int*            out_size) noexcept {
    auto fin = w.finalize_bnet_packet();
    if (!fin.has_value()) return false;
    auto bytes = w.take();
    if (bytes.empty()) return false;
    if (bytes.size() > pvpgn::integration::legacy_bnetd::kSendPacketMaxSize) return false;
    if (static_cast<unsigned int>(bytes.size()) > max_size) return false;
    std::memcpy(out_buf, bytes.data(), bytes.size());
    *out_size = static_cast<unsigned int>(bytes.size());
    return true;
}

}  // namespace

extern "C" int pvpgn_v3_encode_clanmemberlist_reply(unsigned int          cookie,
                                                    unsigned int          member_count,
                                                    char const* const*    names,
                                                    unsigned char const*  statuses,
                                                    unsigned char const*  online_flags,
                                                    char const* const*    online_statuses,
                                                    unsigned char*        out_buf,
                                                    unsigned int          max_size,
                                                    unsigned int*         out_size) noexcept {
    if (out_buf == nullptr || out_size == nullptr) return 0;
    if (member_count != 0u
        && (names == nullptr || statuses == nullptr || online_flags == nullptr)) {
        return 0;
    }

    pvpgn::protocol::Writer w;
    w.begin_bnet_packet(pvpgn::protocol::bnet::kSidClanMemberList);
    w.write_le<std::uint32_t>(cookie);
    if (member_count > 0xFFu) return 0;
    w.write_u8(static_cast<std::uint8_t>(member_count));
    for (unsigned int i = 0; i < member_count; ++i) {
        if (names[i] == nullptr || names[i][0] == '\0') return 0;
        w.write_cstring(std::string_view{names[i]});
        w.write_u8(statuses[i]);
        w.write_u8(online_flags[i]);
        if (online_statuses != nullptr && online_statuses[i] != nullptr) {
            w.write_cstring(std::string_view{online_statuses[i]});
        } else {
            w.write_cstring(std::string_view{""});
        }
    }
    return finalize_into(w, out_buf, max_size, out_size) ? 1 : 0;
}

extern "C" int pvpgn_v3_encode_clan_createreply(unsigned int       cookie,
                                                unsigned char      check_result,
                                                unsigned int       friend_count,
                                                char const* const* friend_names,
                                                unsigned char*     out_buf,
                                                unsigned int       max_size,
                                                unsigned int*      out_size) noexcept {
    if (out_buf == nullptr || out_size == nullptr) return 0;
    if (friend_count != 0u && friend_names == nullptr) return 0;
    if (friend_count > 0xFFu) return 0;

    pvpgn::protocol::Writer w;
    w.begin_bnet_packet(pvpgn::protocol::bnet::kSidClanCreate);
    w.write_le<std::uint32_t>(cookie);
    w.write_u8(check_result);
    w.write_u8(static_cast<std::uint8_t>(friend_count));
    for (unsigned int i = 0; i < friend_count; ++i) {
        if (friend_names[i] == nullptr) return 0;
        w.write_cstring(std::string_view{friend_names[i]});
    }
    return finalize_into(w, out_buf, max_size, out_size) ? 1 : 0;
}

extern "C" int pvpgn_v3_encode_clan_clanack(unsigned char  unknown1,
                                            unsigned int   clantag,
                                            unsigned char  status,
                                            unsigned char* out_buf,
                                            unsigned int   max_size,
                                            unsigned int*  out_size) noexcept {
    if (out_buf == nullptr || out_size == nullptr) return 0;
    pvpgn::protocol::Writer w;
    w.begin_bnet_packet(pvpgn::protocol::bnet::kSidClanClanAck);
    w.write_u8(unknown1);
    w.write_le<std::uint32_t>(clantag);
    w.write_u8(status);
    return finalize_into(w, out_buf, max_size, out_size) ? 1 : 0;
}

extern "C" int pvpgn_v3_send_clan_clanack(void*         conn_ptr,
                                          unsigned char unknown1,
                                          unsigned int  clantag,
                                          unsigned char status) noexcept {
    if (conn_ptr == nullptr) return 0;
    unsigned char buf[16];
    unsigned int  size = 0;
    if (pvpgn_v3_encode_clan_clanack(unknown1, clantag, status,
                                      buf, sizeof(buf), &size) != 1) {
        return 0;
    }
    return ::pvpgn_v3_send_packet_try(conn_ptr, buf, size);
}

extern "C" int pvpgn_v3_encode_clan_quitnotify(unsigned char  status,
                                               unsigned char* out_buf,
                                               unsigned int   max_size,
                                               unsigned int*  out_size) noexcept {
    if (out_buf == nullptr || out_size == nullptr) return 0;
    pvpgn::protocol::Writer w;
    w.begin_bnet_packet(pvpgn::protocol::bnet::kSidClanQuitNotify);
    w.write_u8(status);
    return finalize_into(w, out_buf, max_size, out_size) ? 1 : 0;
}

extern "C" int pvpgn_v3_send_clan_quitnotify(void*         conn_ptr,
                                             unsigned char status) noexcept {
    if (conn_ptr == nullptr) return 0;
    unsigned char buf[8];
    unsigned int  size = 0;
    if (pvpgn_v3_encode_clan_quitnotify(status, buf, sizeof(buf), &size) != 1) {
        return 0;
    }
    return ::pvpgn_v3_send_packet_try(conn_ptr, buf, size);
}
