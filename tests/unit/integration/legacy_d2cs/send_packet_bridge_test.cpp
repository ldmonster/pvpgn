// SPDX-License-Identifier: GPL-2.0-or-later
// Smoke tests for the legacy_d2cs send_packet_bridge dispatcher.
// Pattern parity with tests/.../legacy_bnetd/send_packet_bridge_test.

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "integration/legacy_d2cs/send_packet_bridge.hpp"

namespace ild = pvpgn::integration::legacy_d2cs;

namespace {

struct FakeSink {
    static inline int   return_value = 1;
    static inline int   call_count   = 0;
    static inline void* last_conn    = nullptr;
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

TEST_CASE("d2cs send_packet returns 0 with no handler",
          "[integration][legacy_d2cs][send_packet_bridge]") {
    auto* saved = ild::get_send_packet_handler();
    ild::set_send_packet_handler(nullptr);
    REQUIRE(::pvpgn_v3_d2cs_send_packet_available() == 0);
    int marker = 0;
    unsigned char b[] = {0x01, 0x02, 0x03, 0x04};
    REQUIRE(::pvpgn_v3_d2cs_send_packet(&marker, b, sizeof b) == 0);
    ild::set_send_packet_handler(saved);
}

TEST_CASE("d2cs send_packet rejects null conn/bytes",
          "[integration][legacy_d2cs][send_packet_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_d2cs_send_packet_available() == 1);

    unsigned char b[] = {0x01};
    REQUIRE(::pvpgn_v3_d2cs_send_packet(nullptr, b, sizeof b) == 0);
    int marker = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_packet(&marker, nullptr, 1u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("d2cs send_packet rejects zero-size and oversize",
          "[integration][legacy_d2cs][send_packet_bridge]") {
    ScopedSink scope;
    int marker = 0;
    unsigned char b[1] = {0};

    REQUIRE(::pvpgn_v3_d2cs_send_packet(&marker, b, 0u) == 0);
    REQUIRE(::pvpgn_v3_d2cs_send_packet(
                &marker, b, ild::kSendPacketMaxSize + 1u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("d2cs send_packet forwards bytes and conn pointer verbatim",
          "[integration][legacy_d2cs][send_packet_bridge]") {
    ScopedSink scope;
    int marker = 0;

    unsigned char b[] = {0xde, 0xad, 0xbe, 0xef, 0x01, 0x02};
    REQUIRE(::pvpgn_v3_d2cs_send_packet(&marker, b, sizeof b) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);
    REQUIRE(FakeSink::last_bytes.size() == sizeof b);
    for (std::size_t i = 0; i < sizeof b; ++i) {
        REQUIRE(FakeSink::last_bytes[i] == b[i]);
    }
}

TEST_CASE("d2cs send_packet propagates handler return values",
          "[integration][legacy_d2cs][send_packet_bridge]") {
    ScopedSink scope;
    int marker = 0;
    unsigned char b[] = {0x42};

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_packet(&marker, b, sizeof b) == 0);
    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_d2cs_send_packet(&marker, b, sizeof b) == -1);
    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_d2cs_send_packet(&marker, b, sizeof b) == 1);
}
