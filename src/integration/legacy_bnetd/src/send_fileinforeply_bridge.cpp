// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_fileinforeply_bridge.hpp"

#include <cstdint>
#include <string>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_fileinforeply(void*         conn_ptr,
                                            std::uint32_t type,
                                            std::uint32_t unknown2,
                                            std::uint64_t timestamp,
                                            char const*   filename) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::FileInfoReply m;
    m.type      = type;
    m.unknown2  = unknown2;
    m.timestamp = timestamp;
    m.filename  = (filename != nullptr) ? std::string{filename} : std::string{};

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

extern "C" int pvpgn_v3_send_pingreply(void* conn_ptr) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::Null m;

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
