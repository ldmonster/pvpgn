// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/file/wire_types.hpp"

namespace fw = pvpgn::protocol::file::wire;

TEST_CASE("file::wire constants match legacy",
          "[protocol][file][wire_types]")
{
    REQUIRE(fw::kClientFileReq      == 0x0100);
    REQUIRE(fw::kClientFileReq2     == 0x0200);
    REQUIRE(fw::kClientFileReq3     == 0x0000);
    REQUIRE(fw::kServerFileReply    == 0x0000);
    REQUIRE(fw::kServerFileUnknown1 == 0xdeadbeefu);

    REQUIRE(fw::kWireBytesClientFileReq   == 36);
    REQUIRE(fw::kWireBytesClientFileReq2  == 20);
    REQUIRE(fw::kWireBytesClientFileReq3  == 52);
    REQUIRE(fw::kWireBytesServerFileReply == 24);
}

TEST_CASE("file::wire structs default-init",
          "[protocol][file][wire_types]")
{
    fw::ServerFileReply r{};
    REQUIRE(r.h.size       == 0);
    REQUIRE(r.h.type       == 0);
    REQUIRE(r.filelen      == 0);
    REQUIRE(r.adid         == 0);
    REQUIRE(r.extensiontag == 0);
    REQUIRE(r.timestamp    == 0);

    fw::ClientFileReq3 q{};
    REQUIRE(q.unknown1  == 0);
    REQUIRE(q.timestamp == 0);
    REQUIRE(q.unknown6  == 0);
}
