// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_cdkey_reply_bridge.hpp"

#include <cstdint>
#include <string>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_cdkeyreply(void* conn_ptr,
                                         unsigned int message,
                                         char const* owner) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::CdKeyLegacyReply m;
    m.message    = static_cast<std::uint32_t>(message);
    m.owner_name = (owner != nullptr) ? std::string{owner} : std::string{};

    pvpgn::protocol::Writer w;
    auto enc = pvpgn::protocol::bnet::encode(w, m);
    if (!enc.has_value()) return 0;

    auto bytes = w.take();
    if (bytes.empty()) return 0;
    if (bytes.size() > pvpgn::integration::legacy_bnetd::kSendPacketMaxSize)
        return 0;
    return ::pvpgn_v3_send_packet_try(
        conn_ptr,
        bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}

extern "C" int pvpgn_v3_send_cdkeyreply2(void* conn_ptr,
                                          unsigned int result,
                                          char const* owner) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::CdKey2Reply m;
    m.result = static_cast<std::uint32_t>(result);
    m.owner  = (owner != nullptr) ? std::string{owner} : std::string{};

    pvpgn::protocol::Writer w;
    auto enc = pvpgn::protocol::bnet::encode(w, m);
    if (!enc.has_value()) return 0;

    auto bytes = w.take();
    if (bytes.empty()) return 0;
    if (bytes.size() > pvpgn::integration::legacy_bnetd::kSendPacketMaxSize)
        return 0;
    return ::pvpgn_v3_send_packet_try(
        conn_ptr,
        bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}

extern "C" int pvpgn_v3_send_cdkeyreply3(void* conn_ptr,
                                          unsigned int message,
                                          char const* owner_name) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::CdKey3Reply m;
    m.message    = static_cast<std::uint32_t>(message);
    m.owner_name = (owner_name != nullptr) ? std::string{owner_name}
                                           : std::string{};

    pvpgn::protocol::Writer w;
    auto enc = pvpgn::protocol::bnet::encode(w, m);
    if (!enc.has_value()) return 0;

    auto bytes = w.take();
    if (bytes.empty()) return 0;
    if (bytes.size() > pvpgn::integration::legacy_bnetd::kSendPacketMaxSize)
        return 0;
    return ::pvpgn_v3_send_packet_try(
        conn_ptr,
        bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}
