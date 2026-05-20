// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_d2cs_send_charloginreply`
// (D2CS_CLIENT_CHARLOGINREPLY 0x07).

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "integration/legacy_d2cs/send_charloginreply_bridge.hpp"
#include "integration/legacy_d2cs/send_packet_bridge.hpp"

namespace ild = pvpgn::integration::legacy_d2cs;

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
    ild::SendPacketHandler prev = ild::get_send_packet_handler();
    ScopedSink() noexcept {
        FakeSink::reset();
        ild::set_send_packet_handler(&FakeSink::handler);
    }
    ~ScopedSink() noexcept { ild::set_send_packet_handler(prev); }
};

}  // namespace

TEST_CASE("d2cs send_charloginreply returns 0 with no handler",
          "[integration][legacy_d2cs][send_charloginreply_bridge]") {
    auto* saved = ild::get_send_packet_handler();
    ild::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_charloginreply(&marker, 0u) == 0);
    ild::set_send_packet_handler(saved);
}

TEST_CASE("d2cs send_charloginreply rejects null conn",
          "[integration][legacy_d2cs][send_charloginreply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_d2cs_send_charloginreply(nullptr, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("d2cs send_charloginreply emits 7-byte type 0x07 wire",
          "[integration][legacy_d2cs][send_charloginreply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_charloginreply(&marker, 0x46u) == 1);
    REQUIRE(FakeSink::last_conn == &marker);
    REQUIRE(FakeSink::last_bytes.size() == 7u);
    REQUIRE(FakeSink::last_bytes[0] == 0x07u);
    REQUIRE(FakeSink::last_bytes[1] == 0x00u);
    REQUIRE(FakeSink::last_bytes[2] == 0x07u);  // type D2CS_CLIENT_CHARLOGINREPLY
    REQUIRE(FakeSink::last_bytes[3] == 0x46u);  // reply LE (NOT_FOUND)
    REQUIRE(FakeSink::last_bytes[4] == 0x00u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
}

TEST_CASE("d2cs send_charloginreply propagates handler return values",
          "[integration][legacy_d2cs][send_charloginreply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_charloginreply(&marker, 0u) == 0);
    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_d2cs_send_charloginreply(&marker, 0u) == -1);
    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_d2cs_send_charloginreply(&marker, 0u) == 1);
}
