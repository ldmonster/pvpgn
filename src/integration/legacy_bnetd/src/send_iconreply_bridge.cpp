// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_iconreply_bridge.hpp"

#include <cstdint>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/common/writer.hpp"

namespace {
constexpr std::uint8_t kServerIconReplyCode = 0x2d;
}  // namespace

extern "C" int pvpgn_v3_send_iconreply(void* conn_ptr,
                                       unsigned long long timestamp,
                                       char const* filename) {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::Writer w;
    w.begin_bnet_packet(kServerIconReplyCode);
    w.write_le<std::uint64_t>(static_cast<std::uint64_t>(timestamp));
    w.write_cstring(filename != nullptr ? filename : "");
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
