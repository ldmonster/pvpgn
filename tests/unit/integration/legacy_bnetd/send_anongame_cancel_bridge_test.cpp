// SPDX-License-Identifier: GPL-2.0-or-later
/// Unit tests for pvpgn_v3_send_anongame_cancel.
///
/// Covers:
///   1. no-handler installed -> returns 0
///   2. null conn_ptr -> returns 0
///   3. wire parity: cancel byte = 0x03, count LE
///   4. handler return propagation (1 / 0 / -1)
///   5. zero count edge case
///   6. max count edge case (0xFFFFFFFF)

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "integration/legacy_bnetd/send_anongame_cancel_bridge.hpp"

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

static int fake_handler(void*                conn_ptr,
                        unsigned char const* data,
                        unsigned int         size) noexcept {
    if (g_cap) {
        g_cap->conn = conn_ptr;
        g_cap->bytes.assign(data, data + size);
    }
    return g_cap ? g_cap->ret : 0;
}

struct HandlerGuard {
    explicit HandlerGuard(Capture& cap, int ret_val = 1) {
        cap.ret = ret_val;
        g_cap   = &cap;
        pvpgn_v3_install_send_packet_handler(fake_handler);
    }
    ~HandlerGuard() {
        pvpgn_v3_install_send_packet_handler(nullptr);
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

}  // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("send_anongame_cancel: no handler returns 0",
          "[send_anongame_cancel_bridge]") {
    pvpgn_v3_install_send_packet_handler(nullptr);
    CHECK(pvpgn_v3_send_anongame_cancel(CONN, 1u) == 0);
}

TEST_CASE("send_anongame_cancel: null conn returns 0",
          "[send_anongame_cancel_bridge]") {
    Capture cap;
    HandlerGuard g(cap);
    CHECK(pvpgn_v3_send_anongame_cancel(nullptr, 1u) == 0);
    CHECK(cap.bytes.empty());
}

TEST_CASE("send_anongame_cancel: wire parity",
          "[send_anongame_cancel_bridge]") {
    Capture cap;
    HandlerGuard g(cap);

    // count=7
    int rc = pvpgn_v3_send_anongame_cancel(CONN, 7u);
    CHECK(rc == 1);
    REQUIRE(cap.bytes.size() == 5u);

    // cancel byte = 0x03 (SERVER_FINDANONGAME_CANCEL)
    CHECK(cap.bytes[0] == 0x03u);
    // count = 7 (LE at offset 1)
    CHECK(read_le32(cap.bytes, 1) == 7u);
    // conn pointer forwarded
    CHECK(cap.conn == CONN);
}

TEST_CASE("send_anongame_cancel: zero count",
          "[send_anongame_cancel_bridge]") {
    Capture cap;
    HandlerGuard g(cap);

    int rc = pvpgn_v3_send_anongame_cancel(CONN, 0u);
    CHECK(rc == 1);
    REQUIRE(cap.bytes.size() == 5u);
    CHECK(cap.bytes[0] == 0x03u);
    CHECK(read_le32(cap.bytes, 1) == 0u);
}

TEST_CASE("send_anongame_cancel: max count",
          "[send_anongame_cancel_bridge]") {
    Capture cap;
    HandlerGuard g(cap);

    int rc = pvpgn_v3_send_anongame_cancel(CONN, 0xFFFFFFFFu);
    CHECK(rc == 1);
    REQUIRE(cap.bytes.size() == 5u);
    CHECK(cap.bytes[0] == 0x03u);
    CHECK(read_le32(cap.bytes, 1) == 0xFFFFFFFFu);
}

TEST_CASE("send_anongame_cancel: handler return propagation",
          "[send_anongame_cancel_bridge]") {
    {
        Capture cap;
        HandlerGuard g(cap, 1);
        CHECK(pvpgn_v3_send_anongame_cancel(CONN, 1u) == 1);
    }
    {
        Capture cap;
        HandlerGuard g(cap, 0);
        CHECK(pvpgn_v3_send_anongame_cancel(CONN, 1u) == 0);
    }
    {
        Capture cap;
        HandlerGuard g(cap, -1);
        CHECK(pvpgn_v3_send_anongame_cancel(CONN, 1u) == -1);
    }
}
