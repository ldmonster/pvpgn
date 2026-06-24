// SPDX-License-Identifier: GPL-2.0-or-later
// Wire tests for d2cs->d2gs bridges:
//   AUTHREQ(0x10), AUTHREPLY(0x11), SETGSINFO(0x12),
//   SETINITINFO(0x15), SETCONFFILE(0x16).
// All share 8-byte hdr (size LE16 | type LE16 | seqno LE32).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "app/d2cs/legacy_d2cs_bridges/send_handle_d2gs_bridges.hpp"
#include "app/d2cs/legacy_d2cs_bridges/send_packet_bridge.hpp"

namespace ild = pvpgn::integration::legacy_d2cs;

namespace {

struct FakeSink {
    static inline int call_count = 0;
    static inline void* last_conn = nullptr;
    static inline std::vector<unsigned char> last_bytes{};
    static void reset() noexcept {
        call_count = 0; last_conn = nullptr; last_bytes.clear();
    }
    static int handler(void* c, void const* b, unsigned int n) noexcept {
        ++call_count; last_conn = c;
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

inline void check_header(std::uint16_t exp_size,
                         std::uint16_t exp_type,
                         std::uint32_t exp_seqno) {
    REQUIRE(FakeSink::last_bytes.size() >= 8u);
    REQUIRE(FakeSink::last_bytes[0] == (exp_size & 0xff));
    REQUIRE(FakeSink::last_bytes[1] == ((exp_size >> 8) & 0xff));
    REQUIRE(FakeSink::last_bytes[2] == (exp_type & 0xff));
    REQUIRE(FakeSink::last_bytes[3] == ((exp_type >> 8) & 0xff));
    REQUIRE(FakeSink::last_bytes[4] == (exp_seqno & 0xff));
    REQUIRE(FakeSink::last_bytes[5] == ((exp_seqno >> 8) & 0xff));
    REQUIRE(FakeSink::last_bytes[6] == ((exp_seqno >> 16) & 0xff));
    REQUIRE(FakeSink::last_bytes[7] == ((exp_seqno >> 24) & 0xff));
}

}  // namespace

TEST_CASE("d2cs AUTHREQ to d2gs encodes hdr + sessionnum + signlen + realm\\0",
          "[integration][legacy_d2cs][send_handle_d2gs_bridges]") {
    ScopedSink scope;
    int m = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_authreq_d2gs(&m, 0x00u, 0x12345678u, 0u, "PvPGN") == 1);
    // hdr(8) + 8 + 5 + 1 = 22
    REQUIRE(FakeSink::last_bytes.size() == 22u);
    check_header(22, 0x0010, 0);
    REQUIRE(FakeSink::last_bytes[8]  == 0x78);
    REQUIRE(FakeSink::last_bytes[11] == 0x12);
    REQUIRE(FakeSink::last_bytes[12] == 0x00);
    REQUIRE(FakeSink::last_bytes[16] == 'P');
    REQUIRE(FakeSink::last_bytes[21] == 0x00);
}

TEST_CASE("d2cs AUTHREPLY to d2gs is 12-byte wire",
          "[integration][legacy_d2cs][send_handle_d2gs_bridges]") {
    ScopedSink scope;
    int m = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_authreply_d2gs(&m, 0u, 0u) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 12u);
    check_header(12, 0x0011, 0);
    REQUIRE(FakeSink::last_bytes[8] == 0x00);
}

TEST_CASE("d2cs SETGSINFO to d2gs is 16-byte wire",
          "[integration][legacy_d2cs][send_handle_d2gs_bridges]") {
    ScopedSink scope;
    int m = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_setgsinfo_d2gs(&m, 0u, 0x40u, 0x01u) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 16u);
    check_header(16, 0x0012, 0);
    REQUIRE(FakeSink::last_bytes[8]  == 0x40);
    REQUIRE(FakeSink::last_bytes[12] == 0x01);
}

TEST_CASE("d2cs SETINITINFO to d2gs encodes hdr + 3 u32 + 2 c-strings",
          "[integration][legacy_d2cs][send_handle_d2gs_bridges]") {
    ScopedSink scope;
    int m = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_setinitinfo_d2gs(&m, 0u,
                0x1u, 0x2u, 0u, "ab", "cd") == 1);
    // hdr(8) + 12 + 3 + 3 = 26
    REQUIRE(FakeSink::last_bytes.size() == 26u);
    check_header(26, 0x0015, 0);
    REQUIRE(FakeSink::last_bytes[8]  == 0x01);
    REQUIRE(FakeSink::last_bytes[12] == 0x02);
    REQUIRE(FakeSink::last_bytes[16] == 0x00);
    REQUIRE(FakeSink::last_bytes[20] == 'a');
    REQUIRE(FakeSink::last_bytes[21] == 'b');
    REQUIRE(FakeSink::last_bytes[22] == 0x00);
    REQUIRE(FakeSink::last_bytes[23] == 'c');
    REQUIRE(FakeSink::last_bytes[24] == 'd');
    REQUIRE(FakeSink::last_bytes[25] == 0x00);
}

TEST_CASE("d2cs SETCONFFILE to d2gs encodes hdr + size + reserved + raw bytes",
          "[integration][legacy_d2cs][send_handle_d2gs_bridges]") {
    ScopedSink scope;
    int m = 0;
    unsigned char data[3] = {0x11, 0x22, 0x33};
    REQUIRE(::pvpgn_v3_d2cs_send_setconffile_d2gs(&m, 0u, 0x03u, 0x7fu,
                data, 3u) == 1);
    // hdr(8) + 8 + 3 = 19
    REQUIRE(FakeSink::last_bytes.size() == 19u);
    check_header(19, 0x0016, 0);
    REQUIRE(FakeSink::last_bytes[8]  == 0x03);
    REQUIRE(FakeSink::last_bytes[12] == 0x7f);
    REQUIRE(FakeSink::last_bytes[16] == 0x11);
    REQUIRE(FakeSink::last_bytes[17] == 0x22);
    REQUIRE(FakeSink::last_bytes[18] == 0x33);
}

TEST_CASE("d2cs handle_d2gs bridges reject null inputs",
          "[integration][legacy_d2cs][send_handle_d2gs_bridges]") {
    ScopedSink scope;
    int m = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_authreq_d2gs(nullptr, 0, 0, 0, "x") == 0);
    REQUIRE(::pvpgn_v3_d2cs_send_authreq_d2gs(&m, 0, 0, 0, nullptr) == 0);
    REQUIRE(::pvpgn_v3_d2cs_send_authreply_d2gs(nullptr, 0, 0) == 0);
    REQUIRE(::pvpgn_v3_d2cs_send_setgsinfo_d2gs(nullptr, 0, 0, 0) == 0);
    REQUIRE(::pvpgn_v3_d2cs_send_setinitinfo_d2gs(nullptr, 0, 0, 0, 0, "a", "b") == 0);
    REQUIRE(::pvpgn_v3_d2cs_send_setinitinfo_d2gs(&m, 0, 0, 0, 0, nullptr, "b") == 0);
    REQUIRE(::pvpgn_v3_d2cs_send_setinitinfo_d2gs(&m, 0, 0, 0, 0, "a", nullptr) == 0);
    REQUIRE(::pvpgn_v3_d2cs_send_setconffile_d2gs(nullptr, 0, 0, 0, nullptr, 0) == 0);
    REQUIRE(FakeSink::call_count == 0);
}
