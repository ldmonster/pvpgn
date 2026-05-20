// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_changepassack` (SID 0x31).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_changepassack_bridge.hpp"
#include "integration/legacy_bnetd/send_packet_bridge.hpp"

namespace ila = pvpgn::integration::legacy_bnetd;

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
    ila::SendPacketHandler prev = ila::get_send_packet_handler();
    ScopedSink() noexcept {
        FakeSink::reset();
        ila::set_send_packet_handler(&FakeSink::handler);
    }
    ~ScopedSink() noexcept { ila::set_send_packet_handler(prev); }
};

}  // namespace

TEST_CASE("send_changepassack returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_changepassack_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_changepassack(&marker, 1u) == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_changepassack rejects null conn pointer",
          "[integration][legacy_bnetd][send_changepassack_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_changepassack(nullptr, 1u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_changepassack emits 8-byte SID 0x31 wire format (success)",
          "[integration][legacy_bnetd][send_changepassack_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_changepassack(&marker, 1u) == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + message(4) = 8 bytes
    REQUIRE(FakeSink::last_bytes.size() == 8u);
    REQUIRE(FakeSink::last_bytes[0] == 0xffu);
    REQUIRE(FakeSink::last_bytes[1] == 0x31u);
    REQUIRE(FakeSink::last_bytes[2] == 0x08u);
    REQUIRE(FakeSink::last_bytes[3] == 0x00u);

    // message = 1 LE
    REQUIRE(FakeSink::last_bytes[4] == 0x01u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);
}

TEST_CASE("send_changepassack emits fail message correctly",
          "[integration][legacy_bnetd][send_changepassack_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_changepassack(&marker, 0u) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 8u);
    REQUIRE(FakeSink::last_bytes[4] == 0x00u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);
}

TEST_CASE("send_changepassack propagates handler return values",
          "[integration][legacy_bnetd][send_changepassack_bridge]") {
    ScopedSink scope;
    int marker = 0;
    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_changepassack(&marker, 1u) == 0);
    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_changepassack(&marker, 1u) == -1);
    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_send_changepassack(&marker, 1u) == 1);
}
