// SPDX-License-Identifier: GPL-2.0-or-later
// Wire-byte test for pvpgn_v3_d2dbs_send_echorequest bridge.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "app/d2dbs/legacy_d2dbs_bridges/send_echorequest_bridge.hpp"
#include "app/d2dbs/legacy_d2dbs_bridges/send_packet_bridge.hpp"

namespace ild = pvpgn::integration::legacy_d2dbs;

namespace {

struct FakeSink {
    static inline int call_count = 0;
    static inline void* last_conn = nullptr;
    static inline std::vector<unsigned char> last_bytes{};

    static void reset() noexcept {
        call_count = 0;
        last_conn  = nullptr;
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
        return 1;
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

TEST_CASE("d2dbs echorequest bridge returns 0 without conn",
          "[integration][legacy_d2dbs][send_echorequest_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_d2dbs_send_echorequest(nullptr, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("d2dbs echorequest bridge encodes 8-byte wire (seqno=0)",
          "[integration][legacy_d2dbs][send_echorequest_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_d2dbs_send_echorequest(&marker, 0u) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header: u16 size=8 | u16 type=0x34 | u32 seqno=0 -- all LE.
    REQUIRE(FakeSink::last_bytes.size() == 8u);
    REQUIRE(FakeSink::last_bytes[0] == 0x08);
    REQUIRE(FakeSink::last_bytes[1] == 0x00);
    REQUIRE(FakeSink::last_bytes[2] == 0x34);
    REQUIRE(FakeSink::last_bytes[3] == 0x00);
    REQUIRE(FakeSink::last_bytes[4] == 0x00);
    REQUIRE(FakeSink::last_bytes[5] == 0x00);
    REQUIRE(FakeSink::last_bytes[6] == 0x00);
    REQUIRE(FakeSink::last_bytes[7] == 0x00);
}

TEST_CASE("d2dbs echorequest bridge encodes seqno LE",
          "[integration][legacy_d2dbs][send_echorequest_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_d2dbs_send_echorequest(&marker, 0xdeadbeefu) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 8u);
    // seqno bytes 4..7 LE
    REQUIRE(FakeSink::last_bytes[4] == 0xef);
    REQUIRE(FakeSink::last_bytes[5] == 0xbe);
    REQUIRE(FakeSink::last_bytes[6] == 0xad);
    REQUIRE(FakeSink::last_bytes[7] == 0xde);
}

TEST_CASE("d2dbs echorequest bridge returns 0 when no handler installed",
          "[integration][legacy_d2dbs][send_echorequest_bridge]") {
    auto* saved = ild::get_send_packet_handler();
    ild::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_d2dbs_send_echorequest(&marker, 0u) == 0);
    ild::set_send_packet_handler(saved);
}
