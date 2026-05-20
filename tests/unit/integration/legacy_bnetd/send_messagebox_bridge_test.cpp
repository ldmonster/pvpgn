// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for pvpgn_v3_send_messagebox (SERVER_MESSAGEBOX, SID 0x19).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_messagebox_bridge.hpp"
#include "integration/legacy_bnetd/send_packet_bridge.hpp"

namespace ila = pvpgn::integration::legacy_bnetd;

namespace {

struct FakeSink {
    static inline int call_count = 0;
    static inline void* last_conn = nullptr;
    static inline std::vector<unsigned char> last_bytes{};

    static void reset() noexcept {
        call_count = 0;
        last_conn  = nullptr;
        last_bytes.clear();
    }
    static int handler(void* conn_ptr, void const* bytes, unsigned int size) noexcept {
        ++call_count;
        last_conn = conn_ptr;
        last_bytes.assign(
            static_cast<unsigned char const*>(bytes),
            static_cast<unsigned char const*>(bytes) + size);
        return 1;
    }
};

struct ScopedSink {
    ila::SendPacketHandler prev = ila::get_send_packet_handler();
    ScopedSink() noexcept { FakeSink::reset(); ila::set_send_packet_handler(&FakeSink::handler); }
    ~ScopedSink() noexcept { ila::set_send_packet_handler(prev); }
};

}  // namespace

TEST_CASE("send_messagebox returns 0 with no handler",
          "[integration][legacy_bnetd][send_messagebox_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int m = 0;
    REQUIRE(::pvpgn_v3_send_messagebox(&m, 0u, "hi", "cap") == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_messagebox rejects null inputs",
          "[integration][legacy_bnetd][send_messagebox_bridge]") {
    ScopedSink scope;
    int m = 0;
    REQUIRE(::pvpgn_v3_send_messagebox(nullptr, 0u, "t", "c") == 0);
    REQUIRE(::pvpgn_v3_send_messagebox(&m, 0u, nullptr, "c") == 0);
    REQUIRE(::pvpgn_v3_send_messagebox(&m, 0u, "t", nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_messagebox encodes header + style + cstrings",
          "[integration][legacy_bnetd][send_messagebox_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // style = 0x00000004 (YESNO), text = "hi", caption = "ok"
    REQUIRE(::pvpgn_v3_send_messagebox(&marker, 0x00000004u, "hi", "ok") == 1);
    REQUIRE(FakeSink::last_conn == &marker);
    // Expected: ff 19 <size_LE> | 04 00 00 00 | 'h' 'i' 00 | 'o' 'k' 00
    // size = 4 (hdr) + 4 (style) + 3 (text+nul) + 3 (caption+nul) = 14
    REQUIRE(FakeSink::last_bytes.size() == 14u);
    REQUIRE(FakeSink::last_bytes[0] == 0xff);
    REQUIRE(FakeSink::last_bytes[1] == 0x19);
    REQUIRE(FakeSink::last_bytes[2] == 0x0e);
    REQUIRE(FakeSink::last_bytes[3] == 0x00);
    // style LE
    REQUIRE(FakeSink::last_bytes[4] == 0x04);
    REQUIRE(FakeSink::last_bytes[5] == 0x00);
    REQUIRE(FakeSink::last_bytes[6] == 0x00);
    REQUIRE(FakeSink::last_bytes[7] == 0x00);
    // text "hi\0"
    REQUIRE(FakeSink::last_bytes[8]  == 'h');
    REQUIRE(FakeSink::last_bytes[9]  == 'i');
    REQUIRE(FakeSink::last_bytes[10] == 0x00);
    // caption "ok\0"
    REQUIRE(FakeSink::last_bytes[11] == 'o');
    REQUIRE(FakeSink::last_bytes[12] == 'k');
    REQUIRE(FakeSink::last_bytes[13] == 0x00);
}

TEST_CASE("send_messagebox encodes empty strings as bare NULs",
          "[integration][legacy_bnetd][send_messagebox_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_messagebox(&marker, 0u, "", "") == 1);
    // 4 hdr + 4 style + 1 (text) + 1 (caption) = 10
    REQUIRE(FakeSink::last_bytes.size() == 10u);
    REQUIRE(FakeSink::last_bytes[8] == 0x00);
    REQUIRE(FakeSink::last_bytes[9] == 0x00);
}
