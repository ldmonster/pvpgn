// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/wolgameres/wire_types.hpp"

namespace w = pvpgn::protocol::wolgameres::wire;

TEST_CASE("wolgameres::wire data type codes",
          "[protocol][wolgameres][wire_types]")
{
    REQUIRE(w::kDataTypeByte   == 1);
    REQUIRE(w::kDataTypeBool   == 2);
    REQUIRE(w::kDataTypeTime   == 5);
    REQUIRE(w::kDataTypeInt    == 6);
    REQUIRE(w::kDataTypeString == 7);
    REQUIRE(w::kDataTypeBigint == 20);
}

TEST_CASE("wolgameres::wire top-level tags match legacy",
          "[protocol][wolgameres][wire_types]")
{
    REQUIRE(w::kClientSern == 0x53455223u);   // SER#
    REQUIRE(w::kClientType == 0x54595045u);   // TYPE
    REQUIRE(w::kClientMode == 0x4D4F4445u);   // MODE
    REQUIRE(w::kClientTime == 0x54494D45u);   // TIME
    REQUIRE(w::kClientDsvr == 0x44535652u);   // DSVR
}

TEST_CASE("wolgameres::wire RNDG tags match legacy",
          "[protocol][wolgameres][wire_types]")
{
    REQUIRE(w::kClientPnam == 0x504e414du);
    REQUIRE(w::kClientPloc == 0x504c4f43u);
    REQUIRE(w::kClientTeam == 0x5445414du);
    REQUIRE(w::kClientFlgc == 0x464c4743u);
}

TEST_CASE("wolgameres::wire indexed tag arrays expand to ASCII '0'..'7'",
          "[protocol][wolgameres][wire_types]")
{
    REQUIRE(w::kClientNam[0] == 0x4E414D30u);   // NAM0
    REQUIRE(w::kClientNam[7] == 0x4E414D37u);   // NAM7
    REQUIRE(w::kClientIpa[3] == 0x49504133u);   // IPA3
    REQUIRE(w::kClientCid[5] == 0x43494435u);   // CID5
    REQUIRE(w::kClientCol[1] == 0x434f4c31u);   // COL1
    REQUIRE(w::kClientCmp[7] == 0x434D5037u);   // CMP7
    REQUIRE(w::kClientBlc[4] == 0x424c4334u);   // BLC4
    REQUIRE(w::kClientBlk[6] == 0x424c4b36u);   // BLK6
}

TEST_CASE("wolgameres::wire make_tag and make_indexed_tag helpers",
          "[protocol][wolgameres][wire_types]")
{
    REQUIRE(w::make_tag('A', 'B', 'C', 'D')
            == ((0x41u << 24) | (0x42u << 16) | (0x43u << 8) | 0x44u));
    REQUIRE(w::make_indexed_tag('N', 'A', 'M', 2)
            == 0x4E414D32u);
}
