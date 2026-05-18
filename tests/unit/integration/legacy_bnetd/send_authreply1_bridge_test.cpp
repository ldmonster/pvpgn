// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_authreply1`. The bridge builds the
// SERVER_AUTHREPLY1 bytes via the v3 codec and dispatches through
// the registered send_packet handler. Verifies:
//   - byte-parity vs the legacy on-wire layout for BADVERSION / OK
//     / UPDATE variants.
//   - null `conn_ptr` rejection.
//   - returns 0 when no send_packet handler is installed.
//   - propagates the handler return (1 / 0 / -1).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_authreply1_bridge.hpp"
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

TEST_CASE("send_authreply1 returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_authreply1_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int conn_marker = 0;
    REQUIRE(::pvpgn_v3_send_authreply1(&conn_marker, 2u, nullptr) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_authreply1 rejects null conn pointer",
          "[integration][legacy_bnetd][send_authreply1_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_authreply1(nullptr, 2u, nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_authreply1 OK with no filename emits legacy bytes",
          "[integration][legacy_bnetd][send_authreply1_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_authreply1(&marker, 2u, nullptr) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // Header(FF 07 size_le) + u32 OK(2) + ""\0 + ""\0 = 10 bytes.
    const unsigned char expected[] = {
        0xFF, 0x07, 0x0A, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x00, 0x00
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected,
                        sizeof(expected)) == 0);
}

TEST_CASE("send_authreply1 BADVERSION emits legacy bytes",
          "[integration][legacy_bnetd][send_authreply1_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_authreply1(&marker, 0u, nullptr) == 1);

    const unsigned char expected[] = {
        0xFF, 0x07, 0x0A, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected,
                        sizeof(expected)) == 0);
}

TEST_CASE("send_authreply1 UPDATE prepends filename verbatim",
          "[integration][legacy_bnetd][send_authreply1_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_authreply1(&marker, 2u, "p.mpq") == 1);

    // Header + u32 OK + "p.mpq"\0 + ""\0 + ""\0 = 16 bytes.
    const unsigned char expected[] = {
        0xFF, 0x07, 0x10, 0x00,
        0x02, 0x00, 0x00, 0x00,
        'p',  '.',  'm',  'p',  'q', 0x00,
        0x00,
        0x00
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected,
                        sizeof(expected)) == 0);
}

TEST_CASE("send_authreply1 propagates handler decline (0)",
          "[integration][legacy_bnetd][send_authreply1_bridge]") {
    ScopedSink scope;
    FakeSink::return_value = 0;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_authreply1(&marker, 2u, nullptr) == 0);
    REQUIRE(FakeSink::call_count == 1);
}

TEST_CASE("send_authreply1 propagates handler failure (-1)",
          "[integration][legacy_bnetd][send_authreply1_bridge]") {
    ScopedSink scope;
    FakeSink::return_value = -1;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_authreply1(&marker, 2u, nullptr) == -1);
    REQUIRE(FakeSink::call_count == 1);
}
