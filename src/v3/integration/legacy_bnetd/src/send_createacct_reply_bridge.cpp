// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_createacct_reply_bridge.hpp"

#include <cstdint>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/common/writer.hpp"

namespace {

constexpr std::uint8_t kServerCreateAcctReply1Code = 0x2a;
constexpr std::uint8_t kServerCreateAcctReply2Code = 0x3d;
constexpr std::uint8_t kServerCreateAccountW3Code  = 0x52;

int send_createacct_common(void* conn_ptr,
                           std::uint8_t code,
                           std::uint32_t result) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::Writer w;
    w.begin_bnet_packet(code);
    w.write_le<std::uint32_t>(result);
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

}  // namespace

extern "C" int pvpgn_v3_send_createacctreply1(void* conn_ptr,
                                              unsigned int result) {
    return send_createacct_common(conn_ptr, kServerCreateAcctReply1Code,
                                  static_cast<std::uint32_t>(result));
}

extern "C" int pvpgn_v3_send_createacctreply2(void* conn_ptr,
                                              unsigned int result) {
    return send_createacct_common(conn_ptr, kServerCreateAcctReply2Code,
                                  static_cast<std::uint32_t>(result));
}

extern "C" int pvpgn_v3_send_createaccount_w3(void* conn_ptr,
                                              unsigned int result) {
    return send_createacct_common(conn_ptr, kServerCreateAccountW3Code,
                                  static_cast<std::uint32_t>(result));
}
