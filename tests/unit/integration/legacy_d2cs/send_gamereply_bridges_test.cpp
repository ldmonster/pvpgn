// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "integration/legacy_d2cs/send_creategamereply_bridge.hpp"
#include "integration/legacy_d2cs/send_joingamereply_bridge.hpp"
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

TEST_CASE("d2cs send_creategamereply emits 13-byte type 0x03 wire",
          "[integration][legacy_d2cs][send_creategamereply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_creategamereply(&marker,
                                                  0xAABBu, 0x0001u, 0x0001u,
                                                  0x00u) == 1);
    REQUIRE(FakeSink::last_conn == &marker);
    REQUIRE(FakeSink::last_bytes.size() == 13u);
    REQUIRE(FakeSink::last_bytes[0] == 0x0Du);
    REQUIRE(FakeSink::last_bytes[1] == 0x00u);
    REQUIRE(FakeSink::last_bytes[2] == 0x03u);  // type CREATEGAMEREPLY
    REQUIRE(FakeSink::last_bytes[3] == 0xBBu);  // seqno LE byte 0
    REQUIRE(FakeSink::last_bytes[4] == 0xAAu);
    REQUIRE(FakeSink::last_bytes[5] == 0x01u);  // gameid LE byte 0
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x01u);  // u1 LE byte 0
    REQUIRE(FakeSink::last_bytes[8] == 0x00u);
    REQUIRE(FakeSink::last_bytes[9]  == 0x00u);  // reply LE 4 bytes
    REQUIRE(FakeSink::last_bytes[10] == 0x00u);
    REQUIRE(FakeSink::last_bytes[11] == 0x00u);
    REQUIRE(FakeSink::last_bytes[12] == 0x00u);
}

TEST_CASE("d2cs send_creategamereply guards",
          "[integration][legacy_d2cs][send_creategamereply_bridge]") {
    SECTION("null handler") {
        auto* saved = ild::get_send_packet_handler();
        ild::set_send_packet_handler(nullptr);
        int marker = 0;
        REQUIRE(::pvpgn_v3_d2cs_send_creategamereply(&marker, 0, 0, 0, 0) == 0);
        ild::set_send_packet_handler(saved);
    }
    SECTION("null conn") {
        ScopedSink scope;
        REQUIRE(::pvpgn_v3_d2cs_send_creategamereply(nullptr, 0, 0, 0, 0) == 0);
        REQUIRE(FakeSink::call_count == 0);
    }
    SECTION("propagates -1/0/1") {
        ScopedSink scope;
        int marker = 0;
        FakeSink::return_value = 0;
        REQUIRE(::pvpgn_v3_d2cs_send_creategamereply(&marker, 0, 0, 0, 0) == 0);
        FakeSink::return_value = -1;
        REQUIRE(::pvpgn_v3_d2cs_send_creategamereply(&marker, 0, 0, 0, 0) == -1);
        FakeSink::return_value = 1;
        REQUIRE(::pvpgn_v3_d2cs_send_creategamereply(&marker, 0, 0, 0, 0) == 1);
    }
}

TEST_CASE("d2cs send_joingamereply emits 21-byte type 0x04 wire with BE addr",
          "[integration][legacy_d2cs][send_joingamereply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // addr_host = 0x01020304 -> wire bytes BE: 01 02 03 04
    REQUIRE(::pvpgn_v3_d2cs_send_joingamereply(&marker,
                                                0x1234u, 0x5678u, 0u,
                                                0x01020304u,
                                                0xDEADBEEFu,
                                                0x00u) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 21u);
    REQUIRE(FakeSink::last_bytes[0] == 0x15u);  // total size 21
    REQUIRE(FakeSink::last_bytes[1] == 0x00u);
    REQUIRE(FakeSink::last_bytes[2] == 0x04u);  // type JOINGAMEREPLY
    REQUIRE(FakeSink::last_bytes[3] == 0x34u);  // seqno LE
    REQUIRE(FakeSink::last_bytes[4] == 0x12u);
    REQUIRE(FakeSink::last_bytes[5] == 0x78u);  // gameid LE
    REQUIRE(FakeSink::last_bytes[6] == 0x56u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);  // u1 LE
    REQUIRE(FakeSink::last_bytes[8] == 0x00u);
    // addr is BE on wire (bswap of host 0x01020304 = 04 03 02 01 LE, but written LE
    // after bswap yields wire bytes 01 02 03 04 -- i.e. BE of host value).
    REQUIRE(FakeSink::last_bytes[9]  == 0x01u);
    REQUIRE(FakeSink::last_bytes[10] == 0x02u);
    REQUIRE(FakeSink::last_bytes[11] == 0x03u);
    REQUIRE(FakeSink::last_bytes[12] == 0x04u);
    // token LE
    REQUIRE(FakeSink::last_bytes[13] == 0xEFu);
    REQUIRE(FakeSink::last_bytes[14] == 0xBEu);
    REQUIRE(FakeSink::last_bytes[15] == 0xADu);
    REQUIRE(FakeSink::last_bytes[16] == 0xDEu);
    // reply LE
    REQUIRE(FakeSink::last_bytes[17] == 0x00u);
    REQUIRE(FakeSink::last_bytes[18] == 0x00u);
    REQUIRE(FakeSink::last_bytes[19] == 0x00u);
    REQUIRE(FakeSink::last_bytes[20] == 0x00u);
}

TEST_CASE("d2cs send_joingamereply guards",
          "[integration][legacy_d2cs][send_joingamereply_bridge]") {
    SECTION("null handler") {
        auto* saved = ild::get_send_packet_handler();
        ild::set_send_packet_handler(nullptr);
        int marker = 0;
        REQUIRE(::pvpgn_v3_d2cs_send_joingamereply(&marker, 0, 0, 0,
                                                    0, 0, 0) == 0);
        ild::set_send_packet_handler(saved);
    }
    SECTION("null conn") {
        ScopedSink scope;
        REQUIRE(::pvpgn_v3_d2cs_send_joingamereply(nullptr, 0, 0, 0,
                                                    0, 0, 0) == 0);
        REQUIRE(FakeSink::call_count == 0);
    }
    SECTION("propagates -1/0/1") {
        ScopedSink scope;
        int marker = 0;
        FakeSink::return_value = 0;
        REQUIRE(::pvpgn_v3_d2cs_send_joingamereply(&marker, 0, 0, 0,
                                                    0, 0, 0) == 0);
        FakeSink::return_value = -1;
        REQUIRE(::pvpgn_v3_d2cs_send_joingamereply(&marker, 0, 0, 0,
                                                    0, 0, 0) == -1);
        FakeSink::return_value = 1;
        REQUIRE(::pvpgn_v3_d2cs_send_joingamereply(&marker, 0, 0, 0,
                                                    0, 0, 0) == 1);
    }
}
