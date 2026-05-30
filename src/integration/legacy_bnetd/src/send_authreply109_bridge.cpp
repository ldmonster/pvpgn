// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_authreply109_bridge.hpp"

#include <cstdint>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/common/writer.hpp"

namespace {

// Legacy SERVER_AUTHREPLY_109 code = 0x51ff -> low byte is the
// packet code in the bnet header. We don't reuse a `kSid*` constant
// here because `kSidAuthCheck = 0x51` already maps to the newer
// protocol's AuthCheck message in `protocol/bnet/messages.hpp`;
// adding a second logical name for the same byte would invite
// confusion at decode time.
constexpr std::uint8_t kServerAuthReply109Code = 0x51;

}  // namespace

extern "C" int pvpgn_v3_send_authreply109(void* conn_ptr,
                                          std::uint32_t message,
                                          char const* mpqfilename) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::Writer w;
    w.begin_bnet_packet(kServerAuthReply109Code);
    w.write_le<std::uint32_t>(message);
    if (mpqfilename != nullptr && mpqfilename[0] != '\0') {
        w.write_cstring(mpqfilename);
    }
    // Legacy always appends exactly one trailing NUL-terminated
    // empty string. See `_client_authreq109` in
    // src/bnetd/handle_bnet.cpp.
    w.write_cstring("");

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
