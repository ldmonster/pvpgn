// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_d2cs/send_handle_d2gs_bridges.hpp"

#include <cstdint>
#include <cstring>

#include "integration/legacy_d2cs/send_packet_bridge.hpp"
#include "protocol/common/writer.hpp"
#include "protocol/d2gs/wire_types.hpp"

namespace {

inline void put_header(pvpgn::protocol::Writer& w,
                       std::size_t total_size,
                       std::uint16_t type,
                       std::uint32_t seqno) {
    w.write_le<std::uint16_t>(static_cast<std::uint16_t>(total_size));
    w.write_le<std::uint16_t>(type);
    w.write_le<std::uint32_t>(seqno);
}

inline int flush(void* conn_ptr, pvpgn::protocol::Writer& w) noexcept {
    auto bytes = w.take();
    if (bytes.empty()) return 0;
    if (bytes.size() > pvpgn::integration::legacy_d2cs::kSendPacketMaxSize) {
        return 0;
    }
    return ::pvpgn_v3_d2cs_send_packet_try(
        conn_ptr,
        bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}

}  // namespace

namespace pd2gs = pvpgn::protocol::d2gs::wire;

extern "C" int pvpgn_v3_d2cs_send_authreq_d2gs(void*        conn_ptr,
                                                 unsigned int seqno,
                                                 unsigned int sessionnum,
                                                 unsigned int signlen,
                                                 char const*  realmname) noexcept {
    if (conn_ptr == nullptr) return 0;
    if (realmname == nullptr) return 0;
    const std::size_t name_len = std::strlen(realmname);
    const std::size_t total = 8u + 4u + 4u + name_len + 1u;
    if (total > 0xFFFFu) return 0;
    pvpgn::protocol::Writer w;
    put_header(w, total, pd2gs::kD2csD2gsAuthReq, static_cast<std::uint32_t>(seqno));
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(sessionnum));
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(signlen));
    w.write_cstring(realmname);
    return flush(conn_ptr, w);
}

extern "C" int pvpgn_v3_d2cs_send_authreply_d2gs(void*        conn_ptr,
                                                   unsigned int seqno,
                                                   unsigned int reply) noexcept {
    if (conn_ptr == nullptr) return 0;
    constexpr std::size_t total = 8u + 4u;
    pvpgn::protocol::Writer w;
    put_header(w, total, pd2gs::kD2csD2gsAuthReply, static_cast<std::uint32_t>(seqno));
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(reply));
    return flush(conn_ptr, w);
}

extern "C" int pvpgn_v3_d2cs_send_setgsinfo_d2gs(void*        conn_ptr,
                                                   unsigned int seqno,
                                                   unsigned int maxgame,
                                                   unsigned int gameflag) noexcept {
    if (conn_ptr == nullptr) return 0;
    constexpr std::size_t total = 8u + 4u + 4u;
    pvpgn::protocol::Writer w;
    put_header(w, total, pd2gs::kD2csD2gsSetGsInfo, static_cast<std::uint32_t>(seqno));
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(maxgame));
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(gameflag));
    return flush(conn_ptr, w);
}

extern "C" int pvpgn_v3_d2cs_send_setinitinfo_d2gs(void*        conn_ptr,
                                                     unsigned int seqno,
                                                     unsigned int time_value,
                                                     unsigned int gs_id,
                                                     unsigned int ac_version,
                                                     char const*  ac_checksum,
                                                     char const*  ac_string) noexcept {
    if (conn_ptr == nullptr) return 0;
    if (ac_checksum == nullptr) return 0;
    if (ac_string == nullptr) return 0;
    const std::size_t cs_len = std::strlen(ac_checksum);
    const std::size_t as_len = std::strlen(ac_string);
    const std::size_t total = 8u + 4u + 4u + 4u + cs_len + 1u + as_len + 1u;
    if (total > 0xFFFFu) return 0;
    pvpgn::protocol::Writer w;
    put_header(w, total, pd2gs::kD2csD2gsSetInitInfo, static_cast<std::uint32_t>(seqno));
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(time_value));
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(gs_id));
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(ac_version));
    w.write_cstring(ac_checksum);
    w.write_cstring(ac_string);
    return flush(conn_ptr, w);
}

extern "C" int pvpgn_v3_d2cs_send_setconffile_d2gs(void*        conn_ptr,
                                                     unsigned int seqno,
                                                     unsigned int size_field,
                                                     unsigned int reserved1,
                                                     void const*  data,
                                                     unsigned int data_size) noexcept {
    if (conn_ptr == nullptr) return 0;
    if (data == nullptr && data_size != 0) return 0;
    const std::size_t total = 8u + 4u + 4u + data_size;
    if (total > 0xFFFFu) return 0;
    pvpgn::protocol::Writer w;
    put_header(w, total, pd2gs::kD2csD2gsSetConfFile, static_cast<std::uint32_t>(seqno));
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(size_field));
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(reserved1));
    if (data_size != 0) {
        w.write_bytes(pvpgn::core::ByteView{
            static_cast<std::byte const*>(data), data_size});
    }
    return flush(conn_ptr, w);
}
