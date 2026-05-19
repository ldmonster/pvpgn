// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_clancreateinvitereply`.
//
// `pvpgn_v3_send_clancreateinvitereply` builds SERVER_CLAN_CREATEINVITEREPLY
// (SID_CLAN_CREATEINVITE2, 0x72) bytes via the v3 codec and dispatches through
// the registered send_packet handler.  Verifies:
//   - byte-parity vs the legacy on-wire layout:
//       header(4) + cookie(4) + clan_tag(4) + clan_creator(cstr) + reply(1)
//   - null `conn_ptr` rejection.
//   - returns 0 when no send_packet handler is installed.
//   - propagates the handler return (1 / 0 / -1).

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "integration/legacy_bnetd/send_clancreateinvitereply_bridge.hpp"
#include "integration/legacy_bnetd/send_packet_bridge.hpp"

namespace ila = pvpgn::integration::legacy_bnetd;

namespace {

struct FakeSink {
    static inline int     return_value = 1;
    static inline int     call_count   = 0;
    static inline void*   last_conn    = nullptr;
    static inline std::vector<unsigned char> last_bytes{};

    static void reset() noexcept {
        return_value = 1;
        call_count   = 0;
        last_conn    = nullptr;
        last_bytes.clear();
    }

    static int handler(void* conn_ptr,
                       void const* bytes,
                       unsigned int size) noexcept {
        ++call_count;
        last_conn = conn_ptr;
        last_bytes.assign(
            static_cast<unsigned char const*>(bytes),
            static_cast<unsigned char const*>(bytes) + size);
        return return_value;
    }
};

struct ScopedSink {
    ila::SendPacketHandler prev = ila::get_send_packet_handler();
    ScopedSink() noexcept {
        FakeSink::reset();
        ila::set_send_packet_handler(&FakeSink::handler);
    }
    ~ScopedSink() noexcept { ila::set_send_packet_handler(prev); }
};

}  // namespace

TEST_CASE("send_clancreateinvitereply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_clancreateinvitereply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_clancreateinvitereply(&marker, 0u, 0u, "", 0u) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_clancreateinvitereply rejects null conn pointer",
          "[integration][legacy_bnetd][send_clancreateinvitereply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_clancreateinvitereply(nullptr, 0u, 0u, "", 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_clancreateinvitereply emits correct wire bytes (success reply)",
          "[integration][legacy_bnetd][send_clancreateinvitereply_bridge]") {
    ScopedSink scope;
    int marker = 42;

    // cookie=3, clan_tag=0, clan_creator="Bob", reply=0x06 (accept/success)
    REQUIRE(::pvpgn_v3_send_clancreateinvitereply(
        &marker, 3u, 0u, "Bob", 0x06u) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // Wire layout:
    //   FF 72 <len_lo> <len_hi>   (header, SID=0x72)
    //   03 00 00 00               (cookie=3, LE u32)
    //   00 00 00 00               (clan_tag=0, LE u32)
    //   42 6F 62 00               (clan_creator="Bob\0")
    //   06                        (reply=0x06)
    // Total = 4 + 4 + 4 + 4 + 1 = 17 bytes
    REQUIRE(FakeSink::last_bytes.size() == 17u);
    REQUIRE(FakeSink::last_bytes[0] == 0xFF);  // BNET magic
    REQUIRE(FakeSink::last_bytes[1] == 0x72);  // SID_CLAN_CREATEINVITE2
    REQUIRE(FakeSink::last_bytes[2] == 17u);   // length lo
    REQUIRE(FakeSink::last_bytes[3] == 0x00);  // length hi
    // cookie = 3 (LE)
    REQUIRE(FakeSink::last_bytes[4] == 0x03);
    REQUIRE(FakeSink::last_bytes[5] == 0x00);
    REQUIRE(FakeSink::last_bytes[6] == 0x00);
    REQUIRE(FakeSink::last_bytes[7] == 0x00);
    // clan_tag = 0 (LE)
    REQUIRE(FakeSink::last_bytes[8]  == 0x00);
    REQUIRE(FakeSink::last_bytes[9]  == 0x00);
    REQUIRE(FakeSink::last_bytes[10] == 0x00);
    REQUIRE(FakeSink::last_bytes[11] == 0x00);
    // clan_creator = "Bob\0"
    REQUIRE(FakeSink::last_bytes[12] == 'B');
    REQUIRE(FakeSink::last_bytes[13] == 'o');
    REQUIRE(FakeSink::last_bytes[14] == 'b');
    REQUIRE(FakeSink::last_bytes[15] == 0x00);
    // reply = 0x06 (accept)
    REQUIRE(FakeSink::last_bytes[16] == 0x06);
}

TEST_CASE("send_clancreateinvitereply emits correct wire bytes (decline reply)",
          "[integration][legacy_bnetd][send_clancreateinvitereply_bridge]") {
    ScopedSink scope;
    int marker = 42;

    // reply=0x04 (decline)
    REQUIRE(::pvpgn_v3_send_clancreateinvitereply(
        &marker, 0u, 0u, "", 0x04u) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 14u);
    REQUIRE(FakeSink::last_bytes[1] == 0x72);
    // reply = 0x04
    REQUIRE(FakeSink::last_bytes[13] == 0x04);
}

TEST_CASE("send_clancreateinvitereply propagates handler return value",
          "[integration][legacy_bnetd][send_clancreateinvitereply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_clancreateinvitereply(&marker, 0u, 0u, "", 0u) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_clancreateinvitereply(&marker, 0u, 0u, "", 0u) == 0);
}
