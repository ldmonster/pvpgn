// SPDX-License-Identifier: GPL-2.0-or-later
#include "app/d2cs/legacy_d2cs_bridges/send_listreply_bridges.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "core/bytes.hpp"
#include "app/d2cs/legacy_d2cs_bridges/send_packet_bridge.hpp"
#include "protocol/common/writer.hpp"

namespace {

using pvpgn::protocol::Writer;

inline void put_d2cs_client_header(Writer& w,
                                   std::uint16_t total_size,
                                   std::uint8_t  type) noexcept {
    w.write_le<std::uint16_t>(total_size);
    w.write_le<std::uint8_t>(type);
}

inline int flush(void* conn_ptr, Writer& w) noexcept {
    auto bytes = w.take();
    if (bytes.empty()) return 0;
    if (bytes.size() > pvpgn::integration::legacy_d2cs::kSendPacketMaxSize) {
        return 0;
    }
    return ::pvpgn_v3_d2cs_send_packet(
        conn_ptr, bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}

}  // namespace

extern "C" int pvpgn_v3_d2cs_send_gamelistreply(void*        conn_ptr,
                                                 unsigned int seqno,
                                                 std::uint32_t token,
                                                 unsigned int currchar,
                                                 std::uint32_t gameflag,
                                                 char const*  game_name,
                                                 char const*  game_desc,
                                                 int          terminator) noexcept {
    if (conn_ptr == nullptr) return 0;
    if (game_name == nullptr) game_name = "";
    if (game_desc == nullptr) game_desc = "";

    // hdr(3) + seqno(2) + token(4) + currchar(1) + gameflag(4) = 14
    // + name + 1 + desc + 1 + (terminator ? extra "\0" : 0)
    std::size_t total = 14u + std::strlen(game_name) + 1u
                            + std::strlen(game_desc) + 1u;
    if (terminator) total += 1u;
    if (total > 0xFFFFu) return 0;

    Writer w;
    put_d2cs_client_header(w, static_cast<std::uint16_t>(total), /*type=*/0x05);
    w.write_le<std::uint16_t>(static_cast<std::uint16_t>(seqno));
    w.write_le<std::uint32_t>(token);
    w.write_le<std::uint8_t>(static_cast<std::uint8_t>(currchar));
    w.write_le<std::uint32_t>(gameflag);
    w.write_cstring(game_name);
    w.write_cstring(game_desc);
    if (terminator) {
        // legacy emits an extra empty string for the terminator entry
        w.write_cstring("");
    }
    return flush(conn_ptr, w);
}

extern "C" int pvpgn_v3_d2cs_send_gameinforeply(void*           conn_ptr,
                                                 unsigned int    seqno,
                                                 std::uint32_t   gameflag,
                                                 std::uint32_t   etime,
                                                 unsigned int    charlevel,
                                                 unsigned int    leveldiff,
                                                 unsigned int    maxchar,
                                                 unsigned int    currchar,
                                                 unsigned char const* chclass_array,
                                                 unsigned char const* level_array,
                                                 char const*     game_desc,
                                                 char const* const* char_names) noexcept {
    if (conn_ptr == nullptr) return 0;
    if (chclass_array == nullptr || level_array == nullptr) return 0;
    if (currchar > 0 && char_names == nullptr) return 0;
    if (game_desc == nullptr) game_desc = "";

    // hdr(3) + seqno(2) + gameflag(4) + etime(4)
    // + 4 bytes (charlevel/leveldiff/maxchar/currchar) + 16+16 = 49
    std::size_t total = 49u + std::strlen(game_desc) + 1u;
    for (unsigned i = 0; i < currchar; ++i) {
        if (char_names[i] == nullptr) return 0;
        total += std::strlen(char_names[i]) + 1u;
    }
    if (total > 0xFFFFu) return 0;

    Writer w;
    put_d2cs_client_header(w, static_cast<std::uint16_t>(total), /*type=*/0x06);
    w.write_le<std::uint16_t>(static_cast<std::uint16_t>(seqno));
    w.write_le<std::uint32_t>(gameflag);
    w.write_le<std::uint32_t>(etime);
    w.write_le<std::uint8_t>(static_cast<std::uint8_t>(charlevel));
    w.write_le<std::uint8_t>(static_cast<std::uint8_t>(leveldiff));
    w.write_le<std::uint8_t>(static_cast<std::uint8_t>(maxchar));
    w.write_le<std::uint8_t>(static_cast<std::uint8_t>(currchar));
    w.write_bytes(pvpgn::core::ByteView{
        reinterpret_cast<std::byte const*>(chclass_array), 16u});
    w.write_bytes(pvpgn::core::ByteView{
        reinterpret_cast<std::byte const*>(level_array), 16u});
    w.write_cstring(game_desc);
    for (unsigned i = 0; i < currchar; ++i) {
        w.write_cstring(char_names[i]);
    }
    return flush(conn_ptr, w);
}
