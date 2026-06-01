// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_d2cs/send_charlistreply_bridge.hpp"

#include <vector>

#include "integration/legacy_d2cs/send_packet_bridge.hpp"
#include "protocol/d2cs/charlistreply_encoder.hpp"

namespace clr = pvpgn::protocol::d2cs::charlistreply;

extern "C" int pvpgn_v3_d2cs_send_charlistreply(
    void*                               conn_ptr,
    unsigned int                        maxchar_field,
    pvpgn_v3_d2cs_charlist_entry const* entries,
    unsigned int                        count) noexcept {
    if (conn_ptr == nullptr) return 0;
    if (count > 0 && entries == nullptr) return 0;
    if (maxchar_field > 0xFFFFu) return 0;

    std::vector<clr::CharEntry> v;
    v.reserve(count);
    for (unsigned int i = 0; i < count; ++i) {
        const auto& e = entries[i];
        clr::CharEntry ce;
        ce.charname = (e.charname != nullptr) ? std::string{e.charname}
                                              : std::string{};
        if (e.portrait != nullptr && e.portrait_len != 0u) {
            ce.portrait.assign(
                reinterpret_cast<std::byte const*>(e.portrait),
                reinterpret_cast<std::byte const*>(e.portrait + e.portrait_len));
        }
        v.push_back(std::move(ce));
    }

    auto bytes = clr::encode(static_cast<std::uint16_t>(maxchar_field), v);
    if (bytes.size() >
        pvpgn::integration::legacy_d2cs::kSendPacketMaxSize) {
        return 0;
    }
    return ::pvpgn_v3_d2cs_send_packet(
        conn_ptr, bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}
