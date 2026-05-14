// SPDX-License-Identifier: GPL-2.0-or-later
//
// Initial round-trip tests for the protocol/d2dbs codec scaffold.

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "protocol/d2dbs/codec.hpp"
#include "protocol/common/writer.hpp"

using namespace pvpgn::protocol::d2dbs;
using namespace pvpgn;

namespace {

template <class M, class Decoded>
void round_trip_down(const M& in) {
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto v = decode_d2dbs_to_d2gs(w.view());
    REQUIRE(v.has_value());
    REQUIRE(std::holds_alternative<Decoded>(v.value()));
    REQUIRE(std::get<Decoded>(v.value()) == in);
}

template <class M, class Decoded>
void round_trip_up(const M& in) {
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto v = decode_d2gs_to_d2dbs(w.view());
    REQUIRE(v.has_value());
    REQUIRE(std::holds_alternative<Decoded>(v.value()));
    REQUIRE(std::get<Decoded>(v.value()) == in);
}

}  // namespace

TEST_CASE("d2dbs: header rejects size < 8", "[protocol][d2dbs]") {
    std::byte buf[8] = {};
    // size = 4 (LE), type = 0x34, seqno = 0
    buf[0] = std::byte{0x04};
    buf[2] = std::byte{0x34};
    auto v = parse_header(core::ByteView{buf, 8});
    REQUIRE_FALSE(v.has_value());
    REQUIRE(v.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("d2dbs: 0x34 echo request round-trip", "[protocol][d2dbs]") {
    EchoRequest in{};
    in.seqno = 0xDEADBEEFu;
    round_trip_down<EchoRequest, EchoRequest>(in);
}

TEST_CASE("d2dbs: 0x34 echo reply round-trip", "[protocol][d2dbs]") {
    EchoReply in{};
    in.seqno = 0x00010203u;
    round_trip_up<EchoReply, EchoReply>(in);
}

TEST_CASE("d2dbs: unimplemented downstream type", "[protocol][d2dbs]") {
    std::byte buf[8] = {};
    // size = 8, type = 0x99 (unknown), seqno = 0
    buf[0] = std::byte{0x08};
    buf[2] = std::byte{0x99};
    auto v = decode_d2dbs_to_d2gs(core::ByteView{buf, 8});
    REQUIRE_FALSE(v.has_value());
    REQUIRE(v.error().code() == core::StatusCode::Unimplemented);
}

TEST_CASE("d2dbs: connect handshake encode + decode", "[protocol][d2dbs]") {
    ConnectHandshake in{};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    REQUIRE(w.view().size() == 1);
    REQUIRE(static_cast<std::uint8_t>(w.view()[0]) == kConnectClassD2gsToD2dbs);
    auto parsed = decode_connect_handshake(w.view());
    REQUIRE(parsed.has_value());
    REQUIRE(parsed.value() == in);
}

TEST_CASE("d2dbs: connect handshake rejects unknown class",
          "[protocol][d2dbs]") {
    std::byte b[1] = {std::byte{0x42}};
    auto v = decode_connect_handshake(core::ByteView{b, 1});
    REQUIRE_FALSE(v.has_value());
    REQUIRE(v.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("d2dbs: 0x30 SAVE_DATA request round-trip (charsave + blob)",
          "[protocol][d2dbs]") {
    SaveDataRequest in{};
    in.seqno    = 0x12345678u;
    in.datatype = kDataCharsave;
    in.account  = "moose";
    in.charname = "Squirrel";
    in.data     = {0xDE, 0xAD, 0xBE, 0xEF, 0x42};
    round_trip_up<SaveDataRequest, SaveDataRequest>(in);
}

TEST_CASE("d2dbs: 0x30 SAVE_DATA reply round-trip (success)",
          "[protocol][d2dbs]") {
    SaveDataReply in{};
    in.seqno    = 0xABCDEF01u;
    in.result   = kSaveDataSuccess;
    in.datatype = kDataPortrait;
    in.charname = "Squirrel";
    round_trip_down<SaveDataReply, SaveDataReply>(in);
}

TEST_CASE("d2dbs: 0x31 GET_DATA request round-trip", "[protocol][d2dbs]") {
    GetDataRequest in{};
    in.seqno    = 0x10203040u;
    in.datatype = kDataCharsave;
    in.account  = "moose";
    in.charname = "Squirrel";
    round_trip_up<GetDataRequest, GetDataRequest>(in);
}

TEST_CASE("d2dbs: 0x31 GET_DATA reply round-trip (with blob)",
          "[protocol][d2dbs]") {
    GetDataReply in{};
    in.seqno          = 0x99887766u;
    in.result         = kGetDataSuccess;
    in.charcreatetime = 0x60000000u;
    in.allowladder    = 1;
    in.datatype       = kDataCharsave;
    in.charname       = "Squirrel";
    in.data           = {1, 2, 3, 4, 5, 6, 7, 8};
    round_trip_down<GetDataReply, GetDataReply>(in);
}

TEST_CASE("d2dbs: GET_DATA reply golden header bytes",
          "[protocol][d2dbs][golden]") {
    GetDataReply in{};
    in.seqno          = 0x00000001u;
    in.result         = kGetDataCharLocked;
    in.charcreatetime = 0;
    in.allowladder    = 0;
    in.datatype       = kDataCharsave;
    in.charname       = "X";  // 2 bytes incl NUL
    in.data           = {};

    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    // Total size = 8 hdr + 4 result + 4 cct + 4 al + 2 dt + 2 dl + 2 cn = 26
    REQUIRE(w.view().size() == 26);
    auto v = w.view();
    REQUIRE(static_cast<std::uint8_t>(v[0]) == 0x1A); // size lo = 26
    REQUIRE(static_cast<std::uint8_t>(v[1]) == 0x00);
    REQUIRE(static_cast<std::uint8_t>(v[2]) == 0x31); // type 0x31 lo
    REQUIRE(static_cast<std::uint8_t>(v[3]) == 0x00);
    REQUIRE(static_cast<std::uint8_t>(v[4]) == 0x01); // seqno LE
    REQUIRE(static_cast<std::uint8_t>(v[5]) == 0x00);
    REQUIRE(static_cast<std::uint8_t>(v[6]) == 0x00);
    REQUIRE(static_cast<std::uint8_t>(v[7]) == 0x00);
    REQUIRE(static_cast<std::uint8_t>(v[8]) == 0x02); // result = 2 (CharLocked)
    REQUIRE(static_cast<std::uint8_t>(v[24]) == 'X'); // charname start
    REQUIRE(static_cast<std::uint8_t>(v[25]) == 0x00);
}

TEST_CASE("d2dbs: 0x32 UPDATE_LADDER request round-trip",
          "[protocol][d2dbs]") {
    UpdateLadderRequest in{};
    in.seqno       = 0xAABBCCDDu;
    in.charlevel   = 99;
    in.charexplow  = 0xDEADBEEFu;
    in.charexphigh = 0x00000001u;
    in.charclass   = 5;
    in.charstatus  = 0x0080;  // hardcore
    in.charname    = "Conan";
    in.realmname   = "Asia";
    round_trip_up<UpdateLadderRequest, UpdateLadderRequest>(in);
}

TEST_CASE("d2dbs: 0x33 CHAR_LOCK lock + unlock round-trip",
          "[protocol][d2dbs]") {
    CharLockRequest lock{};
    lock.seqno      = 0x10000000u;
    lock.lockstatus = 1;
    lock.charname   = "Mage";
    lock.realmname  = "EU";
    round_trip_up<CharLockRequest, CharLockRequest>(lock);

    CharLockRequest unlock{};
    unlock.seqno      = 0x10000001u;
    unlock.lockstatus = 0;
    unlock.charname   = "Mage";
    unlock.realmname  = "EU";
    round_trip_up<CharLockRequest, CharLockRequest>(unlock);
}


