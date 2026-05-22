// SPDX-License-Identifier: GPL-2.0-or-later
/// Unit tests for pvpgn_v3_send_anongame_search_reply.
///
/// Covers:
///   1. no-handler installed -> returns 0
///   2. null conn_ptr -> returns 0
///   3. wire parity: option byte = 0x01, count LE, reply LE, search_time LE
///   4. handler return propagation (1 / 0 / -1)
///   5. zero count and zero search_time edge case

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "integration/legacy_bnetd/send_anongame_search_reply_bridge.hpp"

// ---------------------------------------------------------------------------
// Minimal fake handler infrastructure
// ---------------------------------------------------------------------------

namespace {

struct Capture {
    void*                      conn  = nullptr;
    std::vector<unsigned char> bytes;
    int                        ret   = 1;
};

static Capture* g_cap = nullptr;

static int fake_handler(void*        conn_ptr,
                        void const*  data,
                        unsigned int size) noexcept {
    if (g_cap) {
        g_cap->conn = conn_ptr;
        auto bytes = static_cast<unsigned char const*>(data);
        g_cap->bytes.assign(bytes, bytes + size);
    }
    return g_cap ? g_cap->ret : 0;
}

struct HandlerGuard {
    explicit HandlerGuard(Capture& cap, int ret_val = 1) {
        cap.ret = ret_val;
        g_cap   = &cap;
        pvpgn::integration::legacy_bnetd::set_send_packet_handler(fake_handler);
    }
    ~HandlerGuard() {
        pvpgn::integration::legacy_bnetd::set_send_packet_handler(nullptr);
        g_cap = nullptr;
    }
};

static int g_conn_dummy = 0;
static void* CONN = &g_conn_dummy;

// Helper: read a 32-bit little-endian value from a byte buffer.
inline std::uint32_t read_le32(std::vector<unsigned char> const& v, std::size_t off) {
    return static_cast<std::uint32_t>(v[off])
         | (static_cast<std::uint32_t>(v[off+1]) << 8)
         | (static_cast<std::uint32_t>(v[off+2]) << 16)
         | (static_cast<std::uint32_t>(v[off+3]) << 24);
}

// Helper: read a 16-bit little-endian value from a byte buffer.
inline std::uint16_t read_le16(std::vector<unsigned char> const& v, std::size_t off) {
    return static_cast<std::uint16_t>(v[off])
         | static_cast<std::uint16_t>(static_cast<std::uint16_t>(v[off+1]) << 8);
}

}  // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("send_anongame_search_reply: no handler returns 0",
          "[send_anongame_search_reply_bridge]") {
    pvpgn::integration::legacy_bnetd::set_send_packet_handler(nullptr);
    CHECK(pvpgn_v3_send_anongame_search_reply(CONN, 1u, 0u, 5u) == 0);
}

TEST_CASE("send_anongame_search_reply: null conn returns 0",
          "[send_anongame_search_reply_bridge]") {
    Capture cap;
    HandlerGuard g(cap);
    CHECK(pvpgn_v3_send_anongame_search_reply(nullptr, 1u, 0u, 5u) == 0);
    CHECK(cap.bytes.empty());
}

TEST_CASE("send_anongame_search_reply: wire parity",
          "[send_anongame_search_reply_bridge]") {
    Capture cap;
    HandlerGuard g(cap);

    // count=7, reply=0, search_time=42
    int rc = pvpgn_v3_send_anongame_search_reply(CONN, 7u, 0u, 42u);
    CHECK(rc == 1);
    REQUIRE(cap.bytes.size() == 11u);

    // option byte = 0x01 (SERVER_FINDANONGAME_SEARCH)
    CHECK(cap.bytes[0] == 0x01u);
    // count = 7 (LE at offset 1)
    CHECK(read_le32(cap.bytes, 1) == 7u);
    // reply = 0 (LE at offset 5)
    CHECK(read_le32(cap.bytes, 5) == 0u);
    // search_time = 42 (LE at offset 9)
    CHECK(read_le16(cap.bytes, 9) == 42u);
    // conn pointer forwarded
    CHECK(cap.conn == CONN);
}

TEST_CASE("send_anongame_search_reply: non-zero reply field",
          "[send_anongame_search_reply_bridge]") {
    Capture cap;
    HandlerGuard g(cap);

    int rc = pvpgn_v3_send_anongame_search_reply(CONN, 3u, 0xDEADBEEFu, 100u);
    CHECK(rc == 1);
    REQUIRE(cap.bytes.size() == 11u);
    CHECK(read_le32(cap.bytes, 1) == 3u);
    CHECK(read_le32(cap.bytes, 5) == 0xDEADBEEFu);
    CHECK(read_le16(cap.bytes, 9) == 100u);
}

TEST_CASE("send_anongame_search_reply: zero count and zero search_time",
          "[send_anongame_search_reply_bridge]") {
    Capture cap;
    HandlerGuard g(cap);

    int rc = pvpgn_v3_send_anongame_search_reply(CONN, 0u, 0u, 0u);
    CHECK(rc == 1);
    REQUIRE(cap.bytes.size() == 11u);
    CHECK(cap.bytes[0] == 0x01u);
    CHECK(read_le32(cap.bytes, 1) == 0u);
    CHECK(read_le32(cap.bytes, 5) == 0u);
    CHECK(read_le16(cap.bytes, 9) == 0u);
}

TEST_CASE("send_anongame_search_reply: handler return propagation",
          "[send_anongame_search_reply_bridge]") {
    {
        Capture cap;
        HandlerGuard g(cap, 1);
        CHECK(pvpgn_v3_send_anongame_search_reply(CONN, 1u, 0u, 1u) == 1);
    }
    {
        Capture cap;
        HandlerGuard g(cap, 0);
        CHECK(pvpgn_v3_send_anongame_search_reply(CONN, 1u, 0u, 1u) == 0);
    }
    {
        Capture cap;
        HandlerGuard g(cap, -1);
        CHECK(pvpgn_v3_send_anongame_search_reply(CONN, 1u, 0u, 1u) == -1);
    }
}
