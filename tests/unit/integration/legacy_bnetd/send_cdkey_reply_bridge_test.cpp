// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_cdkeyreply`, `pvpgn_v3_send_cdkeyreply2`,
// and `pvpgn_v3_send_cdkeyreply3`. These bridges cover the 3 `packet_create`
// sites in `_client_cdkey`, `_client_cdkey2`, and `_client_cdkey3`
// (handle_bnet.cpp).
//
// Wire-format verification:
//   SERVER_CDKEYREPLY  (0x30): ff 30 size_le + u32 message LE + owner\0
//   SERVER_CDKEYREPLY2 (0x36): ff 36 size_le + u32 result  LE + owner\0
//   SERVER_CDKEYREPLY3 (0x42): ff 42 size_le + u32 message LE + owner_name\0
//
// Also verifies:
//   - null conn_ptr → returns 0 (no crash)
//   - returns 0 when no send_packet handler is installed
//   - propagates handler return values (1 / 0 / -1)
//   - nullptr owner → emits single NUL terminator (empty string)

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_cdkey_reply_bridge.hpp"
#include "integration/legacy_bnetd/send_packet_bridge.hpp"

namespace ila = pvpgn::integration::legacy_bnetd;

namespace {

struct FakeSink {
    static inline int     return_value     = 1;
    static inline int     call_count       = 0;
    static inline void*   last_conn        = nullptr;
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

// ---------------------------------------------------------------------------
// pvpgn_v3_send_cdkeyreply  (SERVER_CDKEYREPLY 0x30)
// ---------------------------------------------------------------------------

TEST_CASE("send_cdkeyreply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_cdkey_reply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_cdkeyreply(&marker, 0x00000001u, "owner") == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_cdkeyreply rejects null conn pointer",
          "[integration][legacy_bnetd][send_cdkey_reply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_cdkeyreply(nullptr, 0x00000001u, "owner") == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_cdkeyreply emits correct wire format with non-empty owner",
          "[integration][legacy_bnetd][send_cdkey_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // message = 0x00000001 (OK), owner = "AB"
    REQUIRE(::pvpgn_v3_send_cdkeyreply(&marker, 0x00000001u, "AB") == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // Total: header(4) + message(4) + "AB\0"(3) = 11 bytes
    REQUIRE(FakeSink::last_bytes.size() == 11u);

    // BNet header: ff SID=0x30 size=0x000b (LE)
    REQUIRE(FakeSink::last_bytes[0] == 0xffu);
    REQUIRE(FakeSink::last_bytes[1] == 0x30u);
    REQUIRE(FakeSink::last_bytes[2] == 0x0bu);
    REQUIRE(FakeSink::last_bytes[3] == 0x00u);

    // message = 0x00000001 LE
    REQUIRE(FakeSink::last_bytes[4] == 0x01u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);

    // owner = "AB\0"
    REQUIRE(FakeSink::last_bytes[8]  == 'A');
    REQUIRE(FakeSink::last_bytes[9]  == 'B');
    REQUIRE(FakeSink::last_bytes[10] == 0x00u);
}

TEST_CASE("send_cdkeyreply with nullptr owner emits empty string (single NUL)",
          "[integration][legacy_bnetd][send_cdkey_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_cdkeyreply(&marker, 0x00000001u, nullptr) == 1);

    // Total: header(4) + message(4) + NUL(1) = 9 bytes
    REQUIRE(FakeSink::last_bytes.size() == 9u);

    // BNet header: ff SID=0x30 size=0x0009 (LE)
    REQUIRE(FakeSink::last_bytes[0] == 0xffu);
    REQUIRE(FakeSink::last_bytes[1] == 0x30u);
    REQUIRE(FakeSink::last_bytes[2] == 0x09u);
    REQUIRE(FakeSink::last_bytes[3] == 0x00u);

    // NUL terminator for empty owner
    REQUIRE(FakeSink::last_bytes[8] == 0x00u);
}

TEST_CASE("send_cdkeyreply propagates handler return values",
          "[integration][legacy_bnetd][send_cdkey_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_cdkeyreply(&marker, 0x00000001u, nullptr) == 0);

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_cdkeyreply(&marker, 0x00000001u, nullptr) == -1);

    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_send_cdkeyreply(&marker, 0x00000001u, nullptr) == 1);
}

// ---------------------------------------------------------------------------
// pvpgn_v3_send_cdkeyreply2  (SERVER_CDKEYREPLY2 0x36)
// ---------------------------------------------------------------------------

TEST_CASE("send_cdkeyreply2 returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_cdkey_reply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_cdkeyreply2(&marker, 0x00000001u, "owner") == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_cdkeyreply2 rejects null conn pointer",
          "[integration][legacy_bnetd][send_cdkey_reply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_cdkeyreply2(nullptr, 0x00000001u, "owner") == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_cdkeyreply2 emits correct wire format with non-empty owner",
          "[integration][legacy_bnetd][send_cdkey_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // result = 0x00000001 (OK), owner = "XY"
    REQUIRE(::pvpgn_v3_send_cdkeyreply2(&marker, 0x00000001u, "XY") == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // Total: header(4) + result(4) + "XY\0"(3) = 11 bytes
    REQUIRE(FakeSink::last_bytes.size() == 11u);

    // BNet header: ff SID=0x36 size=0x000b (LE)
    REQUIRE(FakeSink::last_bytes[0] == 0xffu);
    REQUIRE(FakeSink::last_bytes[1] == 0x36u);
    REQUIRE(FakeSink::last_bytes[2] == 0x0bu);
    REQUIRE(FakeSink::last_bytes[3] == 0x00u);

    // result = 0x00000001 LE
    REQUIRE(FakeSink::last_bytes[4] == 0x01u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);

    // owner = "XY\0"
    REQUIRE(FakeSink::last_bytes[8]  == 'X');
    REQUIRE(FakeSink::last_bytes[9]  == 'Y');
    REQUIRE(FakeSink::last_bytes[10] == 0x00u);
}

TEST_CASE("send_cdkeyreply2 with nullptr owner emits empty string (single NUL)",
          "[integration][legacy_bnetd][send_cdkey_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_cdkeyreply2(&marker, 0x00000001u, nullptr) == 1);

    // Total: header(4) + result(4) + NUL(1) = 9 bytes
    REQUIRE(FakeSink::last_bytes.size() == 9u);

    // BNet header: ff SID=0x36 size=0x0009 (LE)
    REQUIRE(FakeSink::last_bytes[0] == 0xffu);
    REQUIRE(FakeSink::last_bytes[1] == 0x36u);
    REQUIRE(FakeSink::last_bytes[2] == 0x09u);
    REQUIRE(FakeSink::last_bytes[3] == 0x00u);

    // NUL terminator for empty owner
    REQUIRE(FakeSink::last_bytes[8] == 0x00u);
}

TEST_CASE("send_cdkeyreply2 propagates handler return values",
          "[integration][legacy_bnetd][send_cdkey_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_cdkeyreply2(&marker, 0x00000001u, nullptr) == 0);

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_cdkeyreply2(&marker, 0x00000001u, nullptr) == -1);

    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_send_cdkeyreply2(&marker, 0x00000001u, nullptr) == 1);
}

// ---------------------------------------------------------------------------
// pvpgn_v3_send_cdkeyreply3  (SERVER_CDKEYREPLY3 0x42)
// ---------------------------------------------------------------------------

TEST_CASE("send_cdkeyreply3 returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_cdkey_reply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_cdkeyreply3(&marker, 0x00000000u, "owner") == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_cdkeyreply3 rejects null conn pointer",
          "[integration][legacy_bnetd][send_cdkey_reply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_cdkeyreply3(nullptr, 0x00000000u, "owner") == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_cdkeyreply3 emits correct wire format with non-empty owner_name",
          "[integration][legacy_bnetd][send_cdkey_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // message = 0x00000000 (OK), owner_name = "PQ"
    REQUIRE(::pvpgn_v3_send_cdkeyreply3(&marker, 0x00000000u, "PQ") == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // Total: header(4) + message(4) + "PQ\0"(3) = 11 bytes
    REQUIRE(FakeSink::last_bytes.size() == 11u);

    // BNet header: ff SID=0x42 size=0x000b (LE)
    REQUIRE(FakeSink::last_bytes[0] == 0xffu);
    REQUIRE(FakeSink::last_bytes[1] == 0x42u);
    REQUIRE(FakeSink::last_bytes[2] == 0x0bu);
    REQUIRE(FakeSink::last_bytes[3] == 0x00u);

    // message = 0x00000000 LE
    REQUIRE(FakeSink::last_bytes[4] == 0x00u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);

    // owner_name = "PQ\0"
    REQUIRE(FakeSink::last_bytes[8]  == 'P');
    REQUIRE(FakeSink::last_bytes[9]  == 'Q');
    REQUIRE(FakeSink::last_bytes[10] == 0x00u);
}

TEST_CASE("send_cdkeyreply3 with nullptr owner_name emits empty string (single NUL)",
          "[integration][legacy_bnetd][send_cdkey_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_cdkeyreply3(&marker, 0x00000000u, nullptr) == 1);

    // Total: header(4) + message(4) + NUL(1) = 9 bytes
    REQUIRE(FakeSink::last_bytes.size() == 9u);

    // BNet header: ff SID=0x42 size=0x0009 (LE)
    REQUIRE(FakeSink::last_bytes[0] == 0xffu);
    REQUIRE(FakeSink::last_bytes[1] == 0x42u);
    REQUIRE(FakeSink::last_bytes[2] == 0x09u);
    REQUIRE(FakeSink::last_bytes[3] == 0x00u);

    // NUL terminator for empty owner_name
    REQUIRE(FakeSink::last_bytes[8] == 0x00u);
}

TEST_CASE("send_cdkeyreply3 propagates handler return values",
          "[integration][legacy_bnetd][send_cdkey_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_cdkeyreply3(&marker, 0x00000000u, nullptr) == 0);

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_cdkeyreply3(&marker, 0x00000000u, nullptr) == -1);

    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_send_cdkeyreply3(&marker, 0x00000000u, nullptr) == 1);
}
