// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_channellist_bridge.hpp"

#include <cstdint>
#include <cstring>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/common/writer.hpp"

namespace {
constexpr std::uint8_t kServerChannelListCode = 0x0b;
// Mirrors `kChannelListLimit` in the v3 codec.
constexpr unsigned int kChannelListMax = 1024u;
}  // namespace

extern "C" int pvpgn_v3_send_channellist(void* conn_ptr,
                                         char const* const* names,
                                         unsigned int count) {
    if (conn_ptr == nullptr) return 0;
    if (count > kChannelListMax) return 0;
    if (count > 0 && names == nullptr) return 0;

    pvpgn::protocol::Writer w;
    w.begin_bnet_packet(kServerChannelListCode);
    for (unsigned int i = 0; i < count; ++i) {
        char const* n = names[i];
        if (n == nullptr || *n == '\0') {
            // Skip null/empty entries to mirror legacy which never
            // appends an empty name (the trailing "" is the
            // terminator, written below).
            continue;
        }
        w.write_cstring(n);
    }
    // Trailing empty cstring terminator.
    w.write_cstring("");
    auto fin = w.finalize_bnet_packet();
    if (!fin.has_value()) return 0;

    auto bytes = w.take();
    if (bytes.empty()) return 0;
    if (bytes.size() > pvpgn::integration::legacy_bnetd::kSendPacketMaxSize) {
        return 0;
    }
    return ::pvpgn_v3_send_packet_try(
        conn_ptr,
        bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}
