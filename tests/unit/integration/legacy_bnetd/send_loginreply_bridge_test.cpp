// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_loginreply1` and
// `pvpgn_v3_send_loginreply2`. Tiny replies: header + u32 message
// (LOGINREPLY1) and optional cstring reason (LOGINREPLY2).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_loginreply_bridge.hpp"
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

TEST_CASE("send_loginreply1 returns 0 with no handler installed",
          "[integration][legacy_bnetd][send_loginreply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_loginreply1(&marker, 1u) == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_loginreply1 rejects null conn pointer",
          "[integration][legacy_bnetd][send_loginreply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_loginreply1(nullptr, 1u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_loginreply1 success emits legacy 8 bytes",
          "[integration][legacy_bnetd][send_loginreply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_loginreply1(&marker, 0x00000001u) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);
    // FF 29 08 00 01 00 00 00
    const unsigned char expected[] = {
        0xFF, 0x29, 0x08, 0x00,
        0x01, 0x00, 0x00, 0x00
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected, sizeof(expected)) == 0);
}

TEST_CASE("send_loginreply1 fail emits legacy 8 bytes",
          "[integration][legacy_bnetd][send_loginreply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_loginreply1(&marker, 0u) == 1);
    const unsigned char expected[] = {
        0xFF, 0x29, 0x08, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected, sizeof(expected)) == 0);
}

TEST_CASE("send_loginreply2 returns 0 with no handler installed",
          "[integration][legacy_bnetd][send_loginreply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_loginreply2(&marker, 0u, nullptr) == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_loginreply2 rejects null conn pointer",
          "[integration][legacy_bnetd][send_loginreply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_loginreply2(nullptr, 0u, nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_loginreply2 with no reason emits legacy 8 bytes",
          "[integration][legacy_bnetd][send_loginreply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // SUCCESS = 0, no reason.
    REQUIRE(::pvpgn_v3_send_loginreply2(&marker, 0u, nullptr) == 1);
    const unsigned char expected[] = {
        0xFF, 0x3A, 0x08, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected, sizeof(expected)) == 0);
}

TEST_CASE("send_loginreply2 treats empty reason as null",
          "[integration][legacy_bnetd][send_loginreply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_loginreply2(&marker, 2u, "") == 1);
    // Just message=2, no cstring.
    REQUIRE(FakeSink::last_bytes.size() == 8u);
    REQUIRE(FakeSink::last_bytes[1] == 0x3Au);
    REQUIRE(FakeSink::last_bytes[2] == 8u);
    REQUIRE(FakeSink::last_bytes[4] == 0x02u);
}

TEST_CASE("send_loginreply2 LOCKED with reason emits cstring",
          "[integration][legacy_bnetd][send_loginreply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_loginreply2(
                &marker, 0x06u, "locked") == 1);
    // header(4) + u32 LOCKED(4) + "locked\0" (7) = 15 bytes.
    REQUIRE(FakeSink::last_bytes.size() == 15u);
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x3Au);
    REQUIRE(FakeSink::last_bytes[2] == 15u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);
    REQUIRE(FakeSink::last_bytes[4] == 0x06u);
    REQUIRE(FakeSink::last_bytes[5] == 0u);
    REQUIRE(FakeSink::last_bytes[6] == 0u);
    REQUIRE(FakeSink::last_bytes[7] == 0u);
    const char want[] = "locked";
    for (std::size_t i = 0; i < sizeof(want); ++i)
        REQUIRE(FakeSink::last_bytes[8 + i] ==
                static_cast<unsigned char>(want[i]));
}

TEST_CASE("send_loginreply propagates handler return values",
          "[integration][legacy_bnetd][send_loginreply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_loginreply1(&marker, 1u) == 0);
    REQUIRE(::pvpgn_v3_send_loginreply2(&marker, 0u, nullptr) == 0);

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_loginreply1(&marker, 1u) == -1);
    REQUIRE(::pvpgn_v3_send_loginreply2(&marker, 0u, nullptr) == -1);

    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_send_loginreply1(&marker, 1u) == 1);
    REQUIRE(::pvpgn_v3_send_loginreply2(&marker, 0u, "x") == 1);
}
