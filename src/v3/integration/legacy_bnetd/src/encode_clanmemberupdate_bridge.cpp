// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/encode_clanmemberupdate_bridge.hpp"

#include <cstdint>
#include <cstring>
#include <string>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_encode_clanmemberupdate(char const*    name,
                                                unsigned char  status,
                                                unsigned char  online_flag,
                                                char const*    online_status,
                                                unsigned char* out_buf,
                                                unsigned int   max_size,
                                                unsigned int*  out_size) noexcept {
    if (name == nullptr || out_buf == nullptr || out_size == nullptr) return 0;
    if (name[0] == '\0') return 0;

    pvpgn::protocol::Writer w;
    w.begin_bnet_packet(pvpgn::protocol::bnet::kSidClanMemberUpdate);
    w.write_cstring(std::string_view{name});
    w.write_u8(static_cast<std::uint8_t>(status));
    w.write_u8(static_cast<std::uint8_t>(online_flag));
    if (online_status != nullptr) {
        w.write_cstring(std::string_view{online_status});
    } else {
        w.write_cstring(std::string_view{""});
    }
    auto fin = w.finalize_bnet_packet();
    if (!fin.has_value()) return 0;

    auto bytes = w.take();
    if (bytes.empty()) return 0;
    if (bytes.size() > pvpgn::integration::legacy_bnetd::kSendPacketMaxSize) return 0;
    if (static_cast<unsigned int>(bytes.size()) > max_size) return 0;

    std::memcpy(out_buf, bytes.data(), bytes.size());
    *out_size = static_cast<unsigned int>(bytes.size());
    return 1;
}
