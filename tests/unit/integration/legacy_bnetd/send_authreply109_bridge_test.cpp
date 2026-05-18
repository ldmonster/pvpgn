// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_authreply109`. Verifies byte parity
// vs the legacy `_client_authreq109` reply emission across the three
// outcomes (BADVERSION / OK-no-update / OK-with-update), null-conn
// rejection, no-handler fallback, and handler return propagation.

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_authreply109_bridge.hpp"
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

// AUTHREPLY_109 result codes (from src/common/bnet_protocol.h)
constexpr std::uint32_t kAuthReply109MessageOk         = 0x00000000u;
constexpr std::uint32_t kAuthReply109MessageUpdate     = 0x00000100u;
constexpr std::uint32_t kAuthReply109MessageBadVersion = 0x00000101u;

TEST_CASE("send_authreply109 returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_authreply109_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int conn_marker = 0;
    REQUIRE(::pvpgn_v3_send_authreply109(
        &conn_marker, kAuthReply109MessageOk, nullptr) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_authreply109 rejects null conn pointer",
          "[integration][legacy_bnetd][send_authreply109_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_authreply109(
        nullptr, kAuthReply109MessageOk, nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_authreply109 OK no-update emits legacy bytes",
          "[integration][legacy_bnetd][send_authreply109_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_authreply109(
        &marker, kAuthReply109MessageOk, nullptr) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // Reference bytes (bnet_protocol.h):
    //   FF 51 09 00 00 00 00 00 00
    // Header(4) + u32 msg(=0) + ""\0
    const unsigned char expected[] = {
        0xFF, 0x51, 0x09, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected,
                        sizeof(expected)) == 0);
}

TEST_CASE("send_authreply109 BADVERSION emits legacy bytes",
          "[integration][legacy_bnetd][send_authreply109_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_authreply109(
        &marker, kAuthReply109MessageBadVersion, nullptr) == 1);

    // u32 msg=0x101 LE -> 01 01 00 00
    const unsigned char expected[] = {
        0xFF, 0x51, 0x09, 0x00,
        0x01, 0x01, 0x00, 0x00,
        0x00
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected,
                        sizeof(expected)) == 0);
}

TEST_CASE("send_authreply109 UPDATE prepends filename verbatim",
          "[integration][legacy_bnetd][send_authreply109_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // Note: legacy `_client_authreq109` always passes OK as the
    // final message code (the UPDATE assignment is overwritten);
    // the filename is appended *before* the OK overwrite. Mirror
    // that here by passing OK with a non-null filename.
    REQUIRE(::pvpgn_v3_send_authreply109(
        &marker, kAuthReply109MessageOk, "p.mpq") == 1);

    // Header(4) + u32 OK(0) + "p.mpq"\0 + ""\0 = 15 bytes.
    const unsigned char expected[] = {
        0xFF, 0x51, 0x0F, 0x00,
        0x00, 0x00, 0x00, 0x00,
        'p',  '.',  'm',  'p',  'q', 0x00,
        0x00
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected,
                        sizeof(expected)) == 0);
}

TEST_CASE("send_authreply109 empty mpqfilename string is treated as no-update",
          "[integration][legacy_bnetd][send_authreply109_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_authreply109(
        &marker, kAuthReply109MessageOk, "") == 1);

    // Empty pointer-non-null filename must NOT emit a second cstring;
    // it must produce the same 9-byte payload as nullptr filename.
    REQUIRE(FakeSink::last_bytes.size() == 9);
    REQUIRE(static_cast<unsigned char>(FakeSink::last_bytes[2]) == 0x09);
}

TEST_CASE("send_authreply109 propagates handler decline (0)",
          "[integration][legacy_bnetd][send_authreply109_bridge]") {
    ScopedSink scope;
    FakeSink::return_value = 0;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_authreply109(
        &marker, kAuthReply109MessageOk, nullptr) == 0);
    REQUIRE(FakeSink::call_count == 1);
}

TEST_CASE("send_authreply109 propagates handler failure (-1)",
          "[integration][legacy_bnetd][send_authreply109_bridge]") {
    ScopedSink scope;
    FakeSink::return_value = -1;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_authreply109(
        &marker, kAuthReply109MessageOk, nullptr) == -1);
    REQUIRE(FakeSink::call_count == 1);
}
