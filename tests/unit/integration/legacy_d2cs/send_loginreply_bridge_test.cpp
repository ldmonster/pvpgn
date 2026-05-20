// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_d2cs_send_loginreply` (D2CS_CLIENT_LOGINREPLY 0x01).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_d2cs/send_loginreply_bridge.hpp"
#include "integration/legacy_d2cs/send_packet_bridge.hpp"

namespace ild = pvpgn::integration::legacy_d2cs;

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
    ild::SendPacketHandler prev = ild::get_send_packet_handler();
    ScopedSink() noexcept {
        FakeSink::reset();
        ild::set_send_packet_handler(&FakeSink::handler);
    }
    ~ScopedSink() noexcept { ild::set_send_packet_handler(prev); }
};

}  // namespace

TEST_CASE("d2cs send_loginreply returns 0 with no handler",
          "[integration][legacy_d2cs][send_loginreply_bridge]") {
    auto* saved = ild::get_send_packet_handler();
    ild::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_loginreply(&marker, 0u) == 0);
    ild::set_send_packet_handler(saved);
}

TEST_CASE("d2cs send_loginreply rejects null conn",
          "[integration][legacy_d2cs][send_loginreply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_d2cs_send_loginreply(nullptr, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("d2cs send_loginreply emits 7-byte type 0x01 wire (success)",
          "[integration][legacy_d2cs][send_loginreply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_loginreply(&marker, 0u) == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // size LE (u16) + type (u8) + reply LE (u32) = 7 bytes
    REQUIRE(FakeSink::last_bytes.size() == 7u);
    REQUIRE(FakeSink::last_bytes[0] == 0x07u);
    REQUIRE(FakeSink::last_bytes[1] == 0x00u);
    REQUIRE(FakeSink::last_bytes[2] == 0x01u);  // type D2CS_CLIENT_LOGINREPLY
    REQUIRE(FakeSink::last_bytes[3] == 0x00u);
    REQUIRE(FakeSink::last_bytes[4] == 0x00u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
}

TEST_CASE("d2cs send_loginreply emits BADPASS reply variant",
          "[integration][legacy_d2cs][send_loginreply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_loginreply(&marker, 0x0Cu) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 7u);
    REQUIRE(FakeSink::last_bytes[3] == 0x0Cu);  // reply LE byte 0
    REQUIRE(FakeSink::last_bytes[4] == 0x00u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
}

TEST_CASE("d2cs send_loginreply propagates handler return values",
          "[integration][legacy_d2cs][send_loginreply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_loginreply(&marker, 0u) == 0);
    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_d2cs_send_loginreply(&marker, 0u) == -1);
    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_d2cs_send_loginreply(&marker, 0u) == 1);
}
