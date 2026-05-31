// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_logonproof_reply`.
// Wire layout: header(4) + u32 response + 20-byte proof + optional
// NUL-terminated custom_reason string.

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_logonproof_reply_bridge.hpp"
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
    static int handler(void* conn_ptr, void const* bytes,
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

TEST_CASE("send_logonproof_reply returns 0 with no handler installed",
          "[integration][legacy_bnetd][send_logonproof_reply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_logonproof_reply(
        &marker, 0u, nullptr, nullptr) == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_logonproof_reply rejects null conn pointer",
          "[integration][legacy_bnetd][send_logonproof_reply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_logonproof_reply(
        nullptr, 0u, nullptr, nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_logonproof_reply BADPASS with null proof emits 28 zero-tailed bytes",
          "[integration][legacy_bnetd][send_logonproof_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // BADPASS = 0x00000002.
    REQUIRE(::pvpgn_v3_send_logonproof_reply(
        &marker, 0x00000002u, nullptr, nullptr) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);
    REQUIRE(FakeSink::last_bytes.size() == 28u);
    // Header: FF 54 1C 00
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x54u);
    REQUIRE(FakeSink::last_bytes[2] == 0x1Cu);
    REQUIRE(FakeSink::last_bytes[3] == 0x00u);
    // Response: 02 00 00 00
    REQUIRE(FakeSink::last_bytes[4] == 0x02u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);
    // 20-byte proof: all zero.
    for (std::size_t i = 8; i < 28; ++i) {
        REQUIRE(FakeSink::last_bytes[i] == 0x00u);
    }
}

TEST_CASE("send_logonproof_reply OK with explicit proof bytes preserves them",
          "[integration][legacy_bnetd][send_logonproof_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    unsigned char proof[20];
    for (int i = 0; i < 20; ++i) proof[i] = static_cast<unsigned char>(0xB0 + i);
    // OK = 0x00000000.
    REQUIRE(::pvpgn_v3_send_logonproof_reply(
        &marker, 0x00000000u, proof, nullptr) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 28u);
    // Response: 00 00 00 00
    for (std::size_t i = 4; i < 8; ++i) {
        REQUIRE(FakeSink::last_bytes[i] == 0x00u);
    }
    for (std::size_t i = 0; i < 20; ++i) {
        REQUIRE(FakeSink::last_bytes[8 + i]
                == static_cast<unsigned char>(0xB0 + static_cast<int>(i)));
    }
}

TEST_CASE("send_logonproof_reply CUSTOM with reason appends NUL-terminated string",
          "[integration][legacy_bnetd][send_logonproof_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    char const reason[] = "locked";
    // CUSTOM = 0x0000000F.
    REQUIRE(::pvpgn_v3_send_logonproof_reply(
        &marker, 0x0000000Fu, nullptr, reason) == 1);
    // 4 header + 4 response + 20 proof + 6+1 cstring = 35.
    REQUIRE(FakeSink::last_bytes.size() == 35u);
    REQUIRE(FakeSink::last_bytes[1] == 0x54u);
    REQUIRE(FakeSink::last_bytes[2] == 0x23u);  // 35
    REQUIRE(FakeSink::last_bytes[3] == 0x00u);
    REQUIRE(FakeSink::last_bytes[4] == 0x0Fu);
    // Tail: "locked\0".
    REQUIRE(std::memcmp(&FakeSink::last_bytes[28], reason, 7) == 0);
}

TEST_CASE("send_logonproof_reply empty-string reason is treated as no reason",
          "[integration][legacy_bnetd][send_logonproof_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_logonproof_reply(
        &marker, 0u, nullptr, "") == 1);
    REQUIRE(FakeSink::last_bytes.size() == 28u);
}

TEST_CASE("send_logonproof_reply propagates downstream handler return",
          "[integration][legacy_bnetd][send_logonproof_reply_bridge]") {
    ScopedSink scope;
    FakeSink::return_value = -1;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_logonproof_reply(
        &marker, 0u, nullptr, nullptr) == -1);
    REQUIRE(FakeSink::call_count == 1);
}
