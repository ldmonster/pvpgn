// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_clancreateinviteforward`.
//
// `pvpgn_v3_send_clancreateinviteforward` builds SERVER_CLAN_CREATEINVITEREQ
// (SID_CLAN_CREATEINVITE2, 0x72) bytes via the v3 codec and dispatches through
// the registered send_packet handler.  Verifies:
//   - byte-parity vs the legacy on-wire layout:
//       header(4) + cookie(4) + clan_tag(4) + clan_name(cstr) +
//       clan_creator(cstr) + member_count(1) + member_names(cstr[])
//   - null `conn_ptr` rejection.
//   - returns 0 when no send_packet handler is installed.
//   - propagates the handler return (1 / 0 / -1).
//   - null member_names / zero count produces empty friend list.

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_clancreateinviteforward_bridge.hpp"
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

TEST_CASE("send_clancreateinviteforward returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_clancreateinviteforward_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_clancreateinviteforward(
        &marker, 0u, 0u, "", "", nullptr, 0u) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_clancreateinviteforward rejects null conn pointer",
          "[integration][legacy_bnetd][send_clancreateinviteforward_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_clancreateinviteforward(
        nullptr, 0u, 0u, "", "", nullptr, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_clancreateinviteforward emits correct wire bytes (no members)",
          "[integration][legacy_bnetd][send_clancreateinviteforward_bridge]") {
    ScopedSink scope;
    int marker = 42;

    // cookie=1, clan_tag=0x41424344 ('ABCD'), clan_name="MyClan",
    // clan_creator="Bob", no members
    REQUIRE(::pvpgn_v3_send_clancreateinviteforward(
        &marker, 1u, 0x41424344u, "MyClan", "Bob", nullptr, 0u) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // Wire layout:
    //   FF 72 <len_lo> <len_hi>   (header, SID=0x72)
    //   01 00 00 00               (cookie=1, LE u32)
    //   44 43 42 41               (clan_tag=0x41424344, LE u32)
    //   4D 79 43 6C 61 6E 00      (clan_name="MyClan\0")
    //   42 6F 62 00               (clan_creator="Bob\0")
    //   00                        (member_count=0)
    // Total = 4 + 4 + 4 + 7 + 4 + 1 = 24 bytes
    REQUIRE(FakeSink::last_bytes.size() == 24u);
    REQUIRE(FakeSink::last_bytes[0] == 0xFF);  // BNET magic
    REQUIRE(FakeSink::last_bytes[1] == 0x72);  // SID_CLAN_CREATEINVITE2
    REQUIRE(FakeSink::last_bytes[2] == 24u);   // length lo
    REQUIRE(FakeSink::last_bytes[3] == 0x00);  // length hi
    // cookie = 1 (LE)
    REQUIRE(FakeSink::last_bytes[4] == 0x01);
    REQUIRE(FakeSink::last_bytes[5] == 0x00);
    REQUIRE(FakeSink::last_bytes[6] == 0x00);
    REQUIRE(FakeSink::last_bytes[7] == 0x00);
    // clan_tag = 0x41424344 (LE) → bytes 44 43 42 41
    REQUIRE(FakeSink::last_bytes[8]  == 0x44);
    REQUIRE(FakeSink::last_bytes[9]  == 0x43);
    REQUIRE(FakeSink::last_bytes[10] == 0x42);
    REQUIRE(FakeSink::last_bytes[11] == 0x41);
    // clan_name = "MyClan\0"
    REQUIRE(FakeSink::last_bytes[12] == 'M');
    REQUIRE(FakeSink::last_bytes[13] == 'y');
    REQUIRE(FakeSink::last_bytes[14] == 'C');
    REQUIRE(FakeSink::last_bytes[15] == 'l');
    REQUIRE(FakeSink::last_bytes[16] == 'a');
    REQUIRE(FakeSink::last_bytes[17] == 'n');
    REQUIRE(FakeSink::last_bytes[18] == 0x00);
    // clan_creator = "Bob\0"
    REQUIRE(FakeSink::last_bytes[19] == 'B');
    REQUIRE(FakeSink::last_bytes[20] == 'o');
    REQUIRE(FakeSink::last_bytes[21] == 'b');
    REQUIRE(FakeSink::last_bytes[22] == 0x00);
    // member_count = 0
    REQUIRE(FakeSink::last_bytes[23] == 0x00);
}

TEST_CASE("send_clancreateinviteforward emits correct wire bytes (one member)",
          "[integration][legacy_bnetd][send_clancreateinviteforward_bridge]") {
    ScopedSink scope;
    int marker = 42;

    const char* members[] = {"Alice"};
    REQUIRE(::pvpgn_v3_send_clancreateinviteforward(
        &marker, 2u, 0u, "C", "D", members, 1u) == 1);
    REQUIRE(FakeSink::call_count == 1);

    // Wire layout:
    //   FF 72 <len_lo> <len_hi>   (header)
    //   02 00 00 00               (cookie=2)
    //   00 00 00 00               (clan_tag=0)
    //   43 00                     (clan_name="C\0")
    //   44 00                     (clan_creator="D\0")
    //   01                        (member_count=1)
    //   41 6C 69 63 65 00         (member="Alice\0")
    // Total = 4 + 4 + 4 + 2 + 2 + 1 + 6 = 23 bytes
    REQUIRE(FakeSink::last_bytes.size() == 23u);
    REQUIRE(FakeSink::last_bytes[1] == 0x72);
    // member_count = 1
    REQUIRE(FakeSink::last_bytes[16] == 0x01);
    // member name "Alice\0"
    REQUIRE(FakeSink::last_bytes[17] == 'A');
    REQUIRE(FakeSink::last_bytes[18] == 'l');
    REQUIRE(FakeSink::last_bytes[19] == 'i');
    REQUIRE(FakeSink::last_bytes[20] == 'c');
    REQUIRE(FakeSink::last_bytes[21] == 'e');
    REQUIRE(FakeSink::last_bytes[22] == 0x00);
}

TEST_CASE("send_clancreateinviteforward propagates handler return value",
          "[integration][legacy_bnetd][send_clancreateinviteforward_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_clancreateinviteforward(
        &marker, 0u, 0u, "", "", nullptr, 0u) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_clancreateinviteforward(
        &marker, 0u, 0u, "", "", nullptr, 0u) == 0);
}
