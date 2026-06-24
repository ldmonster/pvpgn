// SPDX-License-Identifier: GPL-2.0-or-later
// Wire tests for simple d2cs->client reply bridges.

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "app/d2cs/legacy_d2cs_bridges/send_simple_replies.hpp"
#include "app/d2cs/legacy_d2cs_bridges/send_packet_bridge.hpp"

namespace ild = pvpgn::integration::legacy_d2cs;

namespace {

struct FakeSink {
    static inline int call_count = 0;
    static inline std::vector<unsigned char> last_bytes{};
    static void reset() noexcept { call_count = 0; last_bytes.clear(); }
    static int handler(void*, void const* b, unsigned int n) noexcept {
        ++call_count;
        last_bytes.assign(
            static_cast<unsigned char const*>(b),
            static_cast<unsigned char const*>(b) + n);
        return 1;
    }
};

struct ScopedSink {
    ild::SendPacketHandler prev = ild::get_send_packet_handler();
    ScopedSink() noexcept { FakeSink::reset(); ild::set_send_packet_handler(&FakeSink::handler); }
    ~ScopedSink() noexcept { ild::set_send_packet_handler(prev); }
};

inline void check_hdr(std::uint16_t exp_size, std::uint8_t exp_type) {
    REQUIRE(FakeSink::last_bytes.size() >= 3u);
    REQUIRE(FakeSink::last_bytes[0] == (exp_size & 0xff));
    REQUIRE(FakeSink::last_bytes[1] == ((exp_size >> 8) & 0xff));
    REQUIRE(FakeSink::last_bytes[2] == exp_type);
}

}  // namespace

TEST_CASE("d2cs DELETECHARREPLY 9-byte wire",
          "[integration][legacy_d2cs][send_simple_replies]") {
    ScopedSink scope;
    int m = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_deletecharreply(&m, 0x01u) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 9u);
    check_hdr(9, 0x0a);
    REQUIRE(FakeSink::last_bytes[3] == 0x00);
    REQUIRE(FakeSink::last_bytes[4] == 0x00);
    REQUIRE(FakeSink::last_bytes[5] == 0x01);
}

TEST_CASE("d2cs MOTDREPLY hdr + u1 + cstring",
          "[integration][legacy_d2cs][send_simple_replies]") {
    ScopedSink scope;
    int m = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_motdreply(&m, "hi") == 1);
    REQUIRE(FakeSink::last_bytes.size() == 7u);
    check_hdr(7, 0x12);
    REQUIRE(FakeSink::last_bytes[3] == 0x00);
    REQUIRE(FakeSink::last_bytes[4] == 'h');
    REQUIRE(FakeSink::last_bytes[5] == 'i');
    REQUIRE(FakeSink::last_bytes[6] == 0x00);
}

TEST_CASE("d2cs CREATEGAMEWAIT 7-byte wire",
          "[integration][legacy_d2cs][send_simple_replies]") {
    ScopedSink scope;
    int m = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_creategamewait(&m, 0x42u) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 7u);
    check_hdr(7, 0x14);
    REQUIRE(FakeSink::last_bytes[3] == 0x42);
}

TEST_CASE("d2cs CONVERTCHARREPLY 7-byte wire",
          "[integration][legacy_d2cs][send_simple_replies]") {
    ScopedSink scope;
    int m = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_convertcharreply(&m, 0x01u) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 7u);
    check_hdr(7, 0x18);
    REQUIRE(FakeSink::last_bytes[3] == 0x01);
}

TEST_CASE("d2cs simple replies reject null",
          "[integration][legacy_d2cs][send_simple_replies]") {
    ScopedSink scope;
    int m = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_deletecharreply(nullptr, 0) == 0);
    REQUIRE(::pvpgn_v3_d2cs_send_motdreply(nullptr, "x") == 0);
    REQUIRE(::pvpgn_v3_d2cs_send_motdreply(&m, nullptr) == 0);
    REQUIRE(::pvpgn_v3_d2cs_send_creategamewait(nullptr, 0) == 0);
    REQUIRE(::pvpgn_v3_d2cs_send_convertcharreply(nullptr, 0) == 0);
    REQUIRE(FakeSink::call_count == 0);
}
