// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_authinfo_reply`. The bridge builds
// the SERVER_AUTHREQ_109 (SID_AUTH_INFO reply, 0x50) bytes via the
// v3 codec and dispatches through the registered send_packet
// handler. Verifies:
//   - byte-parity vs the legacy on-wire layout for standard logon
//     and W3 (with 128-byte zero signature pad) variants.
//   - null `conn_ptr` rejection.
//   - returns 0 when no send_packet handler is installed.
//   - propagates the handler return (1 / 0 / -1).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_authinfo_reply_bridge.hpp"
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

TEST_CASE("send_authinfo_reply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_authinfo_reply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_authinfo_reply(
                &marker, 0u, 0u, 0u, 0ull,
                "IX86ver1.mpq", "A=A^S", 0) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_authinfo_reply rejects null conn pointer",
          "[integration][legacy_bnetd][send_authinfo_reply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_authinfo_reply(
                nullptr, 0u, 0u, 0u, 0ull,
                "IX86ver1.mpq", "A=A^S", 0) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_authinfo_reply standard logon emits legacy bytes",
          "[integration][legacy_bnetd][send_authinfo_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // logontype=0, sessionkey=0xdeadbeef, sessionnum=0x12345678,
    // timestamp=0x01c79a4dcafebabe, mpq="IX86ver1.mpq", eq="A=A^S",
    // no W3 signature.
    REQUIRE(::pvpgn_v3_send_authinfo_reply(
                &marker,
                0u, 0xdeadbeefu, 0x12345678u,
                0x01c79a4dcafebabeull,
                "IX86ver1.mpq", "A=A^S", 0) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header (4) + u32*5 (20) + "IX86ver1.mpq\0" (13) +
    // "A=A^S\0" (6) = 43 bytes.
    REQUIRE(FakeSink::last_bytes.size() == 43u);
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x50u);
    REQUIRE(FakeSink::last_bytes[2] == 43u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);
    // logontype = 0
    for (int i = 0; i < 4; ++i)
        REQUIRE(FakeSink::last_bytes[4 + i] == 0u);
    // server_token LE
    REQUIRE(FakeSink::last_bytes[8]  == 0xEFu);
    REQUIRE(FakeSink::last_bytes[9]  == 0xBEu);
    REQUIRE(FakeSink::last_bytes[10] == 0xADu);
    REQUIRE(FakeSink::last_bytes[11] == 0xDEu);
    // session_num LE
    REQUIRE(FakeSink::last_bytes[12] == 0x78u);
    REQUIRE(FakeSink::last_bytes[15] == 0x12u);
    // timestamp low DWORD LE = 0xcafebabe
    REQUIRE(FakeSink::last_bytes[16] == 0xBEu);
    REQUIRE(FakeSink::last_bytes[19] == 0xCAu);
    // timestamp high DWORD LE = 0x01c79a4d
    REQUIRE(FakeSink::last_bytes[20] == 0x4Du);
    REQUIRE(FakeSink::last_bytes[23] == 0x01u);
    // strings
    REQUIRE(FakeSink::last_bytes[24] == 'I');
    REQUIRE(FakeSink::last_bytes[36] == 0u); // NUL after mpq
    REQUIRE(FakeSink::last_bytes[37] == 'A');
    REQUIRE(FakeSink::last_bytes[42] == 0u); // NUL after eq
}

TEST_CASE("send_authinfo_reply W3 includes 128-byte signature pad",
          "[integration][legacy_bnetd][send_authinfo_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_authinfo_reply(
                &marker,
                0x00000002u, // logontype W3
                0u, 0u, 0ull,
                "", "", 1) == 1);
    REQUIRE(FakeSink::call_count == 1);

    // header (4) + u32*5 (20) + "\0" + "\0" + 128 = 154 bytes.
    REQUIRE(FakeSink::last_bytes.size() == 154u);
    REQUIRE(FakeSink::last_bytes[1] == 0x50u);
    REQUIRE(FakeSink::last_bytes[2] == 154u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);
    REQUIRE(FakeSink::last_bytes[4] == 0x02u);
    REQUIRE(FakeSink::last_bytes[24] == 0u);
    REQUIRE(FakeSink::last_bytes[25] == 0u);
    for (std::size_t i = 0; i < 128; ++i)
        REQUIRE(FakeSink::last_bytes[26 + i] == 0u);
}

TEST_CASE("send_authinfo_reply treats null strings as empty",
          "[integration][legacy_bnetd][send_authinfo_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_authinfo_reply(
                &marker, 0u, 0u, 0u, 0ull,
                nullptr, nullptr, 0) == 1);
    // header(4) + u32*5(20) + "\0" + "\0" = 26 bytes.
    REQUIRE(FakeSink::last_bytes.size() == 26u);
    REQUIRE(FakeSink::last_bytes[1] == 0x50u);
    REQUIRE(FakeSink::last_bytes[24] == 0u);
    REQUIRE(FakeSink::last_bytes[25] == 0u);
}

TEST_CASE("send_authinfo_reply propagates handler return values",
          "[integration][legacy_bnetd][send_authinfo_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_authinfo_reply(
                &marker, 0u, 0u, 0u, 0ull,
                "mpq", "eq", 0) == 0);

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_authinfo_reply(
                &marker, 0u, 0u, 0u, 0ull,
                "mpq", "eq", 0) == -1);

    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_send_authinfo_reply(
                &marker, 0u, 0u, 0u, 0ull,
                "mpq", "eq", 0) == 1);
}
