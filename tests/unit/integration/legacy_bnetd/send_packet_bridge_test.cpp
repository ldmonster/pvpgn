// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the dispatcher half of the v3 -> legacy
// send-packet strangler bridge. The legacy-aware sink lives in
// `integration_legacy_bnetd_linked` and is exercised by the
// combined build; here we only verify the C ABI contract: input
// validation, handler dispatch, and the no-handler fallback.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"

namespace ila = pvpgn::integration::legacy_bnetd;

namespace {

// Test scaffold: records every call into the fake handler so the
// test can assert that the C entry-point forwarded exactly the
// inputs it received (or did not forward at all).
struct FakeSink {
    static inline int     return_value     = 1;
    static inline int     call_count       = 0;
    static inline void*   last_conn        = nullptr;
    static inline std::vector<unsigned char> last_bytes{};
    static inline unsigned int last_size   = 0u;

    static void reset() noexcept {
        return_value = 1;
        call_count   = 0;
        last_conn    = nullptr;
        last_bytes.clear();
        last_size    = 0u;
    }

    static int handler(void* conn_ptr,
                       void const* bytes,
                       unsigned int size) noexcept {
        ++call_count;
        last_conn = conn_ptr;
        last_size = size;
        last_bytes.assign(
            static_cast<unsigned char const*>(bytes),
            static_cast<unsigned char const*>(bytes) + size);
        return return_value;
    }
};

struct ScopedHandler {
    ila::SendPacketHandler prev = ila::get_send_packet_handler();
    ScopedHandler() noexcept {
        FakeSink::reset();
        ila::set_send_packet_handler(&FakeSink::handler);
    }
    ~ScopedHandler() noexcept { ila::set_send_packet_handler(prev); }
};

}  // namespace

TEST_CASE("send_packet_try returns 0 when no handler is installed",
          "[integration][legacy_bnetd][send_packet_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int conn_marker = 0;
    unsigned char buf[1] = {0xff};
    REQUIRE(::pvpgn_v3_send_packet(&conn_marker, buf, 1) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_packet_try rejects null conn pointer",
          "[integration][legacy_bnetd][send_packet_bridge]") {
    ScopedHandler scope;
    unsigned char buf[4] = {1, 2, 3, 4};
    REQUIRE(::pvpgn_v3_send_packet(nullptr, buf, 4) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_packet_try rejects null byte pointer",
          "[integration][legacy_bnetd][send_packet_bridge]") {
    ScopedHandler scope;
    int conn_marker = 0;
    REQUIRE(::pvpgn_v3_send_packet(&conn_marker, nullptr, 4) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_packet_try rejects zero size",
          "[integration][legacy_bnetd][send_packet_bridge]") {
    ScopedHandler scope;
    int conn_marker = 0;
    unsigned char buf[1] = {0};
    REQUIRE(::pvpgn_v3_send_packet(&conn_marker, buf, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_packet_try rejects oversize payloads",
          "[integration][legacy_bnetd][send_packet_bridge]") {
    ScopedHandler scope;
    int conn_marker = 0;
    std::vector<unsigned char> buf(ila::kSendPacketMaxSize + 1u, 0xaa);
    REQUIRE(::pvpgn_v3_send_packet(
                &conn_marker, buf.data(),
                static_cast<unsigned int>(buf.size())) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_packet_try forwards a valid payload verbatim",
          "[integration][legacy_bnetd][send_packet_bridge]") {
    ScopedHandler scope;
    int conn_marker = 42;
    unsigned char buf[] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x01, 0x02};
    FakeSink::return_value = 1;

    const int rc = ::pvpgn_v3_send_packet(
        &conn_marker, buf, static_cast<unsigned int>(sizeof(buf)));
    REQUIRE(rc == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &conn_marker);
    REQUIRE(FakeSink::last_size == sizeof(buf));
    REQUIRE(FakeSink::last_bytes
            == std::vector<unsigned char>(buf, buf + sizeof(buf)));
}

TEST_CASE("send_packet_try forwards exactly at the size boundary",
          "[integration][legacy_bnetd][send_packet_bridge]") {
    ScopedHandler scope;
    int conn_marker = 0;
    std::vector<unsigned char> buf(ila::kSendPacketMaxSize, 0x5a);
    FakeSink::return_value = 1;

    const int rc = ::pvpgn_v3_send_packet(
        &conn_marker, buf.data(),
        static_cast<unsigned int>(buf.size()));
    REQUIRE(rc == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_size == ila::kSendPacketMaxSize);
    REQUIRE(FakeSink::last_bytes == buf);
}

TEST_CASE("send_packet_try propagates handler failure return code",
          "[integration][legacy_bnetd][send_packet_bridge]") {
    ScopedHandler scope;
    int conn_marker = 0;
    unsigned char buf[3] = {1, 2, 3};
    FakeSink::return_value = 0;  // simulate legacy enqueue failure

    REQUIRE(::pvpgn_v3_send_packet(&conn_marker, buf, 3) == 0);
    REQUIRE(FakeSink::call_count == 1);
}
