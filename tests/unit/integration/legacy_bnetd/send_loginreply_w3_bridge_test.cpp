// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_loginreply_w3`. Fixed 72-byte
// reply: header + u32 message + 32 bytes salt + 32 bytes
// server_public_key.

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_loginreply_w3_bridge.hpp"
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

TEST_CASE("send_loginreply_w3 returns 0 with no handler installed",
          "[integration][legacy_bnetd][send_loginreply_w3_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_loginreply_w3(
                &marker, 0u, nullptr, nullptr) == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_loginreply_w3 rejects null conn pointer",
          "[integration][legacy_bnetd][send_loginreply_w3_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_loginreply_w3(
                nullptr, 0u, nullptr, nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_loginreply_w3 BADACCT emits 72 zero-padded bytes",
          "[integration][legacy_bnetd][send_loginreply_w3_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // Message=1 (BADACCT/ALREADY), null salt and key -> all zeros.
    REQUIRE(::pvpgn_v3_send_loginreply_w3(
                &marker, 0x00000001u, nullptr, nullptr) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + u32(4) + 32 + 32 = 72 bytes.
    REQUIRE(FakeSink::last_bytes.size() == 72u);
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x53u);
    REQUIRE(FakeSink::last_bytes[2] == 72u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);
    REQUIRE(FakeSink::last_bytes[4] == 0x01u);
    REQUIRE(FakeSink::last_bytes[5] == 0u);
    REQUIRE(FakeSink::last_bytes[6] == 0u);
    REQUIRE(FakeSink::last_bytes[7] == 0u);
    for (std::size_t i = 8; i < 72; ++i)
        REQUIRE(FakeSink::last_bytes[i] == 0u);
}

TEST_CASE("send_loginreply_w3 SUCCESS emits salt + B verbatim",
          "[integration][legacy_bnetd][send_loginreply_w3_bridge]") {
    ScopedSink scope;
    int marker = 0;

    unsigned char salt[32];
    unsigned char skey[32];
    for (unsigned i = 0; i < 32; ++i) {
        salt[i] = static_cast<unsigned char>(0x10u + i);
        skey[i] = static_cast<unsigned char>(0xA0u + i);
    }
    REQUIRE(::pvpgn_v3_send_loginreply_w3(
                &marker, 0u, salt, skey) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 72u);
    REQUIRE(FakeSink::last_bytes[1] == 0x53u);
    // message=0
    REQUIRE(FakeSink::last_bytes[4] == 0u);
    REQUIRE(FakeSink::last_bytes[5] == 0u);
    REQUIRE(FakeSink::last_bytes[6] == 0u);
    REQUIRE(FakeSink::last_bytes[7] == 0u);
    // salt
    for (std::size_t i = 0; i < 32; ++i)
        REQUIRE(FakeSink::last_bytes[8 + i] == salt[i]);
    // server_public_key
    for (std::size_t i = 0; i < 32; ++i)
        REQUIRE(FakeSink::last_bytes[40 + i] == skey[i]);
}

TEST_CASE("send_loginreply_w3 propagates handler return values",
          "[integration][legacy_bnetd][send_loginreply_w3_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_loginreply_w3(
                &marker, 0u, nullptr, nullptr) == 0);

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_loginreply_w3(
                &marker, 0u, nullptr, nullptr) == -1);

    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_send_loginreply_w3(
                &marker, 0u, nullptr, nullptr) == 1);
}
