// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_passchange_bridge.hpp"

#include <cstdint>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_passchangereply(
    void*                conn_ptr,
    unsigned int         message,
    unsigned char const* salt,
    unsigned char const* server_public_key) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::PassChangeReply m;
    m.message = static_cast<std::uint32_t>(message);

    if (salt != nullptr) {
        for (std::size_t i = 0; i < m.salt.size(); ++i)
            m.salt[i] = salt[i];
    }
    if (server_public_key != nullptr) {
        for (std::size_t i = 0; i < m.server_public_key.size(); ++i)
            m.server_public_key[i] = server_public_key[i];
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

extern "C" int pvpgn_v3_send_passchangeproofreply(
    void*                conn_ptr,
    unsigned int         response,
    unsigned char const* server_password_proof) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::PassChangeProofReply m;
    m.response = static_cast<std::uint32_t>(response);

    if (server_password_proof != nullptr) {
        for (std::size_t i = 0; i < m.server_password_proof.size(); ++i)
            m.server_password_proof[i] = server_password_proof[i];
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
