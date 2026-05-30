// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_logonproof_reply_bridge.hpp"

#include <cstdint>

#include "core/bytes.hpp"
#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/common/writer.hpp"

namespace {

// SERVER_LOGONPROOFREPLY = 0x54.
constexpr std::uint8_t kServerLogonProofReplyCode = 0x54;
constexpr std::size_t kProofSize = 20;

}  // namespace

extern "C" int pvpgn_v3_send_logonproof_reply(
    void* conn_ptr,
    unsigned int response,
    unsigned char const* server_password_proof,
    char const* custom_reason) {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::Writer w;
    w.begin_bnet_packet(kServerLogonProofReplyCode);
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(response));

    if (server_password_proof != nullptr) {
        w.write_bytes(pvpgn::core::ByteView{
            reinterpret_cast<std::byte const*>(server_password_proof),
            kProofSize});
    } else {
        std::byte zero[kProofSize]{};
        w.write_bytes(pvpgn::core::ByteView{zero, kProofSize});
    }

    if (custom_reason != nullptr && custom_reason[0] != '\0') {
        w.write_cstring(custom_reason);
    }

    auto fin = w.finalize_bnet_packet();
    if (!fin.has_value()) return 0;

    auto bytes = w.take();
    if (bytes.empty()) return 0;
    if (bytes.size() > pvpgn::integration::legacy_bnetd::kSendPacketMaxSize) {
        return 0;
    }
    return ::pvpgn_v3_send_packet(
        conn_ptr,
        bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}
