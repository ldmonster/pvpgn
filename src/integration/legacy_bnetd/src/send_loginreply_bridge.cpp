// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_loginreply_bridge.hpp"

#include <cstdint>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/common/writer.hpp"

namespace {

// SERVER_LOGINREPLY1 = 0x29, SERVER_LOGINREPLY2 = 0x3A.
// We do NOT route through the v3 `LoginReply1` / `LogonResponse2Reply`
// codec entries -- those have their own trailing-string heuristics
// (emit on `result == 0x06`) which don't perfectly match the legacy
// `supports_locked_reply`-gated emission. Building the bytes directly
// via `Writer` keeps the bridge faithful to legacy wire output.
constexpr std::uint8_t kServerLoginReply1Code = 0x29;
constexpr std::uint8_t kServerLoginReply2Code = 0x3A;

int send_loginreply_common(void* conn_ptr,
                           std::uint8_t code,
                           std::uint32_t message,
                           char const* reason) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::Writer w;
    w.begin_bnet_packet(code);
    w.write_le<std::uint32_t>(message);
    if (reason != nullptr && reason[0] != '\0') {
        w.write_cstring(reason);
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

}  // namespace

extern "C" int pvpgn_v3_send_loginreply1(void* conn_ptr,
                                         std::uint32_t message) noexcept {
    // LOGINREPLY1 never carries a reason string.
    return send_loginreply_common(
        conn_ptr, kServerLoginReply1Code, message, nullptr);
}

extern "C" int pvpgn_v3_send_loginreply2(void* conn_ptr,
                                         std::uint32_t message,
                                         char const* reason) noexcept {
    return send_loginreply_common(
        conn_ptr, kServerLoginReply2Code, message, reason);
}
