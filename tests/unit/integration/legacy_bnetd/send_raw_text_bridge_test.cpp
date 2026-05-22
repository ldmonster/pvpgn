// SPDX-License-Identifier: GPL-2.0-or-later
/// Unit tests for pvpgn_v3_send_raw_text and pvpgn_v3_send_raw_text2.
///
/// Covers:
///   1. no-handler installed -> returns 0
///   2. null conn_ptr -> returns 0
///   3. null text -> returns 0
///   4. empty text -> sends 0 bytes, handler called
///   5. single-string wire parity ("\r\nPassword: ")
///   6. two-string concatenation wire parity (prefix + suffix)
///   7. null prefix treated as ""
///   8. null suffix treated as ""
///   9. handler return propagation (1 / 0 / -1)

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "integration/legacy_bnetd/send_raw_text_bridge.hpp"

// ---------------------------------------------------------------------------
// Minimal fake handler infrastructure (mirrors other bridge tests)
// ---------------------------------------------------------------------------

namespace {

struct Capture {
    void*                      conn  = nullptr;
    std::vector<unsigned char> bytes;
    int                        ret   = 1;
};

static Capture* g_cap = nullptr;

static int fake_handler(void* conn_ptr,
                        void const* data,
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

// Dummy non-null connection pointer (never dereferenced in bridge code).
static int g_conn_dummy = 0;
static void* CONN = &g_conn_dummy;

} // namespace

// ---------------------------------------------------------------------------
// Tests for pvpgn_v3_send_raw_text
// ---------------------------------------------------------------------------

TEST_CASE("send_raw_text: no handler returns 0", "[send_raw_text_bridge]") {
    // Ensure no handler is installed.
    pvpgn::integration::legacy_bnetd::set_send_packet_handler(nullptr);
    CHECK(pvpgn_v3_send_raw_text(CONN, "hello") == 0);
}

TEST_CASE("send_raw_text: null conn returns 0", "[send_raw_text_bridge]") {
    Capture cap;
    HandlerGuard g(cap);
    CHECK(pvpgn_v3_send_raw_text(nullptr, "hello") == 0);
    CHECK(cap.bytes.empty());
}

TEST_CASE("send_raw_text: null text returns 0", "[send_raw_text_bridge]") {
    Capture cap;
    HandlerGuard g(cap);
    CHECK(pvpgn_v3_send_raw_text(CONN, nullptr) == 0);
    CHECK(cap.bytes.empty());
}

TEST_CASE("send_raw_text: empty text sends 0 bytes", "[send_raw_text_bridge]") {
    Capture cap;
    HandlerGuard g(cap);
    int rc = pvpgn_v3_send_raw_text(CONN, "");
    // Handler is called with size=0; fake_handler returns 1.
    CHECK(rc == 1);
    CHECK(cap.conn == CONN);
    CHECK(cap.bytes.empty());
}

TEST_CASE("send_raw_text: wire parity for password prompt", "[send_raw_text_bridge]") {
    // The bot/telnet handler sends "\r\nPassword: " as the password prompt.
    char const* msg = "\r\nPassword: ";
    std::size_t const expected_len = std::strlen(msg);

    Capture cap;
    HandlerGuard g(cap);
    int rc = pvpgn_v3_send_raw_text(CONN, msg);
    CHECK(rc == 1);
    REQUIRE(cap.bytes.size() == expected_len);
    CHECK(std::memcmp(cap.bytes.data(), msg, expected_len) == 0);
}

TEST_CASE("send_raw_text: wire parity for login-failed message", "[send_raw_text_bridge]") {
    char const* msg = "\r\nLogin failed.\r\n\r\nUsername: ";
    std::size_t const expected_len = std::strlen(msg);

    Capture cap;
    HandlerGuard g(cap);
    int rc = pvpgn_v3_send_raw_text(CONN, msg);
    CHECK(rc == 1);
    REQUIRE(cap.bytes.size() == expected_len);
    CHECK(std::memcmp(cap.bytes.data(), msg, expected_len) == 0);
}

TEST_CASE("send_raw_text: wire parity for login-success CRLF", "[send_raw_text_bridge]") {
    char const* msg = "\r\n";
    Capture cap;
    HandlerGuard g(cap);
    int rc = pvpgn_v3_send_raw_text(CONN, msg);
    CHECK(rc == 1);
    REQUIRE(cap.bytes.size() == 2u);
    CHECK(cap.bytes[0] == '\r');
    CHECK(cap.bytes[1] == '\n');
}

TEST_CASE("send_raw_text: handler return propagation", "[send_raw_text_bridge]") {
    {
        Capture cap;
        HandlerGuard g(cap, 1);
        CHECK(pvpgn_v3_send_raw_text(CONN, "x") == 1);
    }
    {
        Capture cap;
        HandlerGuard g(cap, 0);
        CHECK(pvpgn_v3_send_raw_text(CONN, "x") == 0);
    }
    {
        Capture cap;
        HandlerGuard g(cap, -1);
        CHECK(pvpgn_v3_send_raw_text(CONN, "x") == -1);
    }
}

// ---------------------------------------------------------------------------
// Tests for pvpgn_v3_send_raw_text2
// ---------------------------------------------------------------------------

TEST_CASE("send_raw_text2: no handler returns 0", "[send_raw_text_bridge]") {
    pvpgn::integration::legacy_bnetd::set_send_packet_handler(nullptr);
    CHECK(pvpgn_v3_send_raw_text2(CONN, "a", "b") == 0);
}

TEST_CASE("send_raw_text2: null conn returns 0", "[send_raw_text_bridge]") {
    Capture cap;
    HandlerGuard g(cap);
    CHECK(pvpgn_v3_send_raw_text2(nullptr, "a", "b") == 0);
    CHECK(cap.bytes.empty());
}

TEST_CASE("send_raw_text2: concatenates prefix and suffix", "[send_raw_text_bridge]") {
    // Bot handler: sends username + "\r\nPassword: " as one packet.
    char const* prefix = "myuser";
    char const* suffix = "\r\nPassword: ";
    std::string expected = std::string(prefix) + suffix;

    Capture cap;
    HandlerGuard g(cap);
    int rc = pvpgn_v3_send_raw_text2(CONN, prefix, suffix);
    CHECK(rc == 1);
    REQUIRE(cap.bytes.size() == expected.size());
    CHECK(std::memcmp(cap.bytes.data(), expected.data(), expected.size()) == 0);
}

TEST_CASE("send_raw_text2: null prefix treated as empty", "[send_raw_text_bridge]") {
    char const* suffix = "\r\nPassword: ";
    std::size_t const expected_len = std::strlen(suffix);

    Capture cap;
    HandlerGuard g(cap);
    int rc = pvpgn_v3_send_raw_text2(CONN, nullptr, suffix);
    CHECK(rc == 1);
    REQUIRE(cap.bytes.size() == expected_len);
    CHECK(std::memcmp(cap.bytes.data(), suffix, expected_len) == 0);
}

TEST_CASE("send_raw_text2: null suffix treated as empty", "[send_raw_text_bridge]") {
    char const* prefix = "myuser";
    std::size_t const expected_len = std::strlen(prefix);

    Capture cap;
    HandlerGuard g(cap);
    int rc = pvpgn_v3_send_raw_text2(CONN, prefix, nullptr);
    CHECK(rc == 1);
    REQUIRE(cap.bytes.size() == expected_len);
    CHECK(std::memcmp(cap.bytes.data(), prefix, expected_len) == 0);
}

TEST_CASE("send_raw_text2: both null -> sends 0 bytes", "[send_raw_text_bridge]") {
    Capture cap;
    HandlerGuard g(cap);
    int rc = pvpgn_v3_send_raw_text2(CONN, nullptr, nullptr);
    CHECK(rc == 1);
    CHECK(cap.bytes.empty());
}

TEST_CASE("send_raw_text2: handler return propagation", "[send_raw_text_bridge]") {
    {
        Capture cap;
        HandlerGuard g(cap, 1);
        CHECK(pvpgn_v3_send_raw_text2(CONN, "a", "b") == 1);
    }
    {
        Capture cap;
        HandlerGuard g(cap, -1);
        CHECK(pvpgn_v3_send_raw_text2(CONN, "a", "b") == -1);
    }
}
