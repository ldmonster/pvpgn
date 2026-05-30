// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_authinfo_reply_bridge.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_authinfo_reply(
    void* conn_ptr,
    std::uint32_t logontype,
    std::uint32_t server_token,
    std::uint32_t session_num,
    std::uint64_t timestamp,
    char const* mpq_filename,
    char const* checksum_formula,
    int include_w3_signature) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::AuthInfoReply m;
    m.logontype        = logontype;
    m.server_token     = server_token;
    m.session_num      = session_num;
    m.timestamp        = timestamp;
    m.mpq_filename     = mpq_filename     != nullptr
                             ? std::string{mpq_filename}
                             : std::string{};
    m.checksum_formula = checksum_formula != nullptr
                             ? std::string{checksum_formula}
                             : std::string{};
    if (include_w3_signature != 0) {
        m.server_signature.assign(128, 0u);
    }

    pvpgn::protocol::Writer w;
    auto enc = pvpgn::protocol::bnet::encode(w, m);
    if (!enc.has_value()) return 0;

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
