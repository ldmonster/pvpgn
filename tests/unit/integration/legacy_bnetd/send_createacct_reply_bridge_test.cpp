// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_createacctreply1`/`2`.
// Wire layout: header(4) + u32 result -> 8 bytes total.

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_createacct_reply_bridge.hpp"
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

TEST_CASE("send_createacctreply1 declines with no handler installed",
          "[integration][legacy_bnetd][send_createacct_reply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_createacctreply1(&marker, 1u) == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_createacctreply1 rejects null conn pointer",
          "[integration][legacy_bnetd][send_createacct_reply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_createacctreply1(nullptr, 1u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_createacctreply1 OK emits 8 bytes (code 0x2a)",
          "[integration][legacy_bnetd][send_createacct_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // OK = 0x00000001.
    REQUIRE(::pvpgn_v3_send_createacctreply1(&marker, 0x00000001u) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);
    const unsigned char expected[] = {
        0xFF, 0x2A, 0x08, 0x00,
        0x01, 0x00, 0x00, 0x00
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected, sizeof(expected)) == 0);
}

TEST_CASE("send_createacctreply1 NO emits 8 bytes",
          "[integration][legacy_bnetd][send_createacct_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_createacctreply1(&marker, 0u) == 1);
    const unsigned char expected[] = {
        0xFF, 0x2A, 0x08, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected, sizeof(expected)) == 0);
}

TEST_CASE("send_createacctreply2 declines with no handler installed",
          "[integration][legacy_bnetd][send_createacct_reply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_createacctreply2(&marker, 4u) == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_createacctreply2 rejects null conn pointer",
          "[integration][legacy_bnetd][send_createacct_reply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_createacctreply2(nullptr, 4u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_createacctreply2 EXIST emits 8 bytes (code 0x3d)",
          "[integration][legacy_bnetd][send_createacct_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // EXIST = 0x00000004.
    REQUIRE(::pvpgn_v3_send_createacctreply2(&marker, 0x00000004u) == 1);
    const unsigned char expected[] = {
        0xFF, 0x3D, 0x08, 0x00,
        0x04, 0x00, 0x00, 0x00
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected, sizeof(expected)) == 0);
}

TEST_CASE("send_createacctreply2 OK emits 8 bytes",
          "[integration][legacy_bnetd][send_createacct_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_createacctreply2(&marker, 0u) == 1);
    const unsigned char expected[] = {
        0xFF, 0x3D, 0x08, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected, sizeof(expected)) == 0);
}

TEST_CASE("send_createacctreply propagates downstream handler return",
          "[integration][legacy_bnetd][send_createacct_reply_bridge]") {
    ScopedSink scope;
    FakeSink::return_value = -1;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_createacctreply1(&marker, 0u) == -1);
    REQUIRE(::pvpgn_v3_send_createacctreply2(&marker, 0u) == -1);
    REQUIRE(FakeSink::call_count == 2);
}

TEST_CASE("send_createaccount_w3 declines with no handler installed",
          "[integration][legacy_bnetd][send_createacct_reply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_createaccount_w3(&marker, 0u) == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_createaccount_w3 rejects null conn pointer",
          "[integration][legacy_bnetd][send_createacct_reply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_createaccount_w3(nullptr, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_createaccount_w3 OK emits 8 bytes (code 0x52)",
          "[integration][legacy_bnetd][send_createacct_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_createaccount_w3(&marker, 0u) == 1);
    const unsigned char expected[] = {
        0xFF, 0x52, 0x08, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected, sizeof(expected)) == 0);
}

TEST_CASE("send_createaccount_w3 EXIST emits 8 bytes",
          "[integration][legacy_bnetd][send_createacct_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // EXIST = 0x00000003.
    REQUIRE(::pvpgn_v3_send_createaccount_w3(&marker, 0x00000003u) == 1);
    const unsigned char expected[] = {
        0xFF, 0x52, 0x08, 0x00,
        0x03, 0x00, 0x00, 0x00
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected, sizeof(expected)) == 0);
}
