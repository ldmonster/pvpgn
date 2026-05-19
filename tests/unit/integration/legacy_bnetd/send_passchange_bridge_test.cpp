// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_passchangereply` and
// `pvpgn_v3_send_passchangeproofreply`.
//
// `pvpgn_v3_send_passchangereply` builds SERVER_PASSCHANGEREPLY
// (SID_PASSCHANGE, 0x55) bytes via the v3 codec and dispatches through the
// registered send_packet handler.  Verifies:
//   - byte-parity vs the legacy on-wire layout:
//       header(4) + message u32(4) + salt[32] + server_public_key[32] = 72 B.
//   - null `conn_ptr` rejection.
//   - returns 0 when no send_packet handler is installed.
//   - propagates the handler return (1 / 0 / -1).
//   - null salt / server_public_key treated as all-zeros.
//
// `pvpgn_v3_send_passchangeproofreply` builds SERVER_PASSCHANGEPROOFREPLY
// (SID_PASSCHANGEPROOF, 0x56) bytes via the v3 codec.  Verifies:
//   - byte-parity vs the legacy on-wire layout:
//       header(4) + response u32(4) + server_password_proof[20] = 28 B.
//   - null `conn_ptr` rejection.
//   - returns 0 when no send_packet handler is installed.
//   - propagates the handler return (1 / 0 / -1).
//   - null server_password_proof treated as all-zeros.

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_passchange_bridge.hpp"
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

// ---------------------------------------------------------------------------
// pvpgn_v3_send_passchangereply tests
// ---------------------------------------------------------------------------

TEST_CASE("send_passchangereply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_passchange_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_passchangereply(
                &marker, 0u, nullptr, nullptr) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_passchangereply rejects null conn pointer",
          "[integration][legacy_bnetd][send_passchange_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_passchangereply(
                nullptr, 0u, nullptr, nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_passchangereply emits correct wire bytes (ACCEPT, all-zero salt/spk)",
          "[integration][legacy_bnetd][send_passchange_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // message=0 (ACCEPT), salt=all-zeros, server_public_key=all-zeros
    REQUIRE(::pvpgn_v3_send_passchangereply(
                &marker, 0u, nullptr, nullptr) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + message(4) + salt(32) + server_public_key(32) = 72 bytes
    REQUIRE(FakeSink::last_bytes.size() == 72u);

    // header: FF 55 48 00  (0x48 = 72)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x55u);
    REQUIRE(FakeSink::last_bytes[2] == 72u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // message LE = 0x00000000
    REQUIRE(FakeSink::last_bytes[4] == 0x00u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);

    // salt[0..31] all zero
    for (int i = 0; i < 32; ++i)
        REQUIRE(FakeSink::last_bytes[8 + i] == 0x00u);

    // server_public_key[0..31] all zero
    for (int i = 0; i < 32; ++i)
        REQUIRE(FakeSink::last_bytes[40 + i] == 0x00u);
}

TEST_CASE("send_passchangereply emits correct wire bytes (REJECT, non-zero salt/spk)",
          "[integration][legacy_bnetd][send_passchange_bridge]") {
    ScopedSink scope;
    int marker = 0;

    unsigned char salt[32]{};
    unsigned char spk[32]{};
    for (int i = 0; i < 32; ++i) {
        salt[i] = static_cast<unsigned char>(i + 1);
        spk[i]  = static_cast<unsigned char>(i + 33);
    }

    // message=1 (REJECT)
    REQUIRE(::pvpgn_v3_send_passchangereply(
                &marker, 1u, salt, spk) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 72u);

    // header: FF 55 48 00
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x55u);

    // message LE = 0x00000001
    REQUIRE(FakeSink::last_bytes[4] == 0x01u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);

    // salt bytes
    for (int i = 0; i < 32; ++i)
        REQUIRE(FakeSink::last_bytes[8 + i] == static_cast<unsigned char>(i + 1));

    // server_public_key bytes
    for (int i = 0; i < 32; ++i)
        REQUIRE(FakeSink::last_bytes[40 + i] == static_cast<unsigned char>(i + 33));
}

TEST_CASE("send_passchangereply propagates handler return value",
          "[integration][legacy_bnetd][send_passchange_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_passchangereply(
                &marker, 0u, nullptr, nullptr) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_passchangereply(
                &marker, 0u, nullptr, nullptr) == 0);
}

// ---------------------------------------------------------------------------
// pvpgn_v3_send_passchangeproofreply tests
// ---------------------------------------------------------------------------

TEST_CASE("send_passchangeproofreply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_passchange_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_passchangeproofreply(
                &marker, 0u, nullptr) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_passchangeproofreply rejects null conn pointer",
          "[integration][legacy_bnetd][send_passchange_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_passchangeproofreply(
                nullptr, 0u, nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_passchangeproofreply emits correct wire bytes (OK, all-zero proof)",
          "[integration][legacy_bnetd][send_passchange_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // response=0 (OK), server_password_proof=all-zeros
    REQUIRE(::pvpgn_v3_send_passchangeproofreply(
                &marker, 0u, nullptr) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + response(4) + server_password_proof(20) = 28 bytes
    REQUIRE(FakeSink::last_bytes.size() == 28u);

    // header: FF 56 1C 00  (0x1C = 28)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x56u);
    REQUIRE(FakeSink::last_bytes[2] == 28u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // response LE = 0x00000000
    REQUIRE(FakeSink::last_bytes[4] == 0x00u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);

    // server_password_proof[0..19] all zero
    for (int i = 0; i < 20; ++i)
        REQUIRE(FakeSink::last_bytes[8 + i] == 0x00u);
}

TEST_CASE("send_passchangeproofreply emits correct wire bytes (BADPASS, non-zero proof)",
          "[integration][legacy_bnetd][send_passchange_bridge]") {
    ScopedSink scope;
    int marker = 0;

    unsigned char proof[20]{};
    for (int i = 0; i < 20; ++i)
        proof[i] = static_cast<unsigned char>(0xA0 + i);

    // response=2 (BADPASS)
    REQUIRE(::pvpgn_v3_send_passchangeproofreply(
                &marker, 2u, proof) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 28u);

    // header: FF 56 1C 00
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x56u);

    // response LE = 0x00000002
    REQUIRE(FakeSink::last_bytes[4] == 0x02u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);

    // server_password_proof bytes
    for (int i = 0; i < 20; ++i)
        REQUIRE(FakeSink::last_bytes[8 + i] == static_cast<unsigned char>(0xA0 + i));
}

TEST_CASE("send_passchangeproofreply propagates handler return value",
          "[integration][legacy_bnetd][send_passchange_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_passchangeproofreply(
                &marker, 0u, nullptr) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_passchangeproofreply(
                &marker, 0u, nullptr) == 0);
}
