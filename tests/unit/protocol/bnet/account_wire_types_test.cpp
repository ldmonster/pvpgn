// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/account_wire_types.hpp"

namespace a = pvpgn::protocol::bnet::account;

TEST_CASE("account packet type codes match legacy",
          "[protocol][bnet][account_wire_types]")
{
    REQUIRE(a::kClientCreateAcctReq1       == 0x2aff);
    REQUIRE(a::kServerCreateAcctReply1     == 0x2aff);
    REQUIRE(a::kClientUnknown2b            == 0x2bff);
    REQUIRE(a::kClientLoginReq2            == 0x3aff);
    REQUIRE(a::kServerLoginReply2          == 0x3aff);
    REQUIRE(a::kClientLoginReqW3           == 0x53ff);
    REQUIRE(a::kServerLoginReplyW3         == 0x53ff);
    REQUIRE(a::kClientLogonProofReq        == 0x54ff);
    REQUIRE(a::kServerLogonProofReply      == 0x54ff);
    REQUIRE(a::kClientPassChangeReq        == 0x55ff);
    REQUIRE(a::kServerPassChangeReply      == 0x55ff);
    REQUIRE(a::kClientPassChangeProofReq   == 0x56ff);
    REQUIRE(a::kServerPassChangeProofReply == 0x56ff);
    REQUIRE(a::kClientCreateAccountW3      == 0x52ff);
    REQUIRE(a::kServerCreateAccountW3      == 0x52ff);
    REQUIRE(a::kClientChangeGamePort       == 0x45ff);
    REQUIRE(a::kClientCreateAcctReq2       == 0x3dff);
    REQUIRE(a::kServerCreateAcctReply2     == 0x3dff);
    REQUIRE(a::kClientUdpOk                == 0x14ff);
    REQUIRE(a::kClientLoginReq1            == 0x29ff);
    REQUIRE(a::kServerLoginReply1          == 0x29ff);
    REQUIRE(a::kClientChangePassReq        == 0x31ff);
    REQUIRE(a::kServerChangePassAck        == 0x31ff);
    REQUIRE(a::kClientSetEmailReply        == 0x59ff);
    REQUIRE(a::kServerSetEmailReq          == 0x59ff);
    REQUIRE(a::kClientGetPasswordReq       == 0x5aff);
    REQUIRE(a::kClientChangeEmailReq       == 0x5bff);
}

TEST_CASE("account result-code enums match legacy",
          "[protocol][bnet][account_wire_types]")
{
    REQUIRE(a::kCreateAcctReply1ResultOk == 0x00000001);
    REQUIRE(a::kCreateAcctReply1ResultNo == 0x00000000);

    REQUIRE(a::kLoginReply2MessageSuccess  == 0x00000000);
    REQUIRE(a::kLoginReply2MessageNonExist == 0x00000001);
    REQUIRE(a::kLoginReply2MessageBadPass  == 0x00000002);
    REQUIRE(a::kLoginReply2MessageLocked   == 0x00000006);

    REQUIRE(a::kLogonProofReplyResponseOk      == 0x00000000);
    REQUIRE(a::kLogonProofReplyResponseBadPass == 0x00000002);
    REQUIRE(a::kLogonProofReplyResponseEmail   == 0x0000000E);
    REQUIRE(a::kLogonProofReplyResponseCustom  == 0x0000000F);

    REQUIRE(a::kPassChangeReplyMessageAccept == 0x00000000);
    REQUIRE(a::kPassChangeReplyMessageReject == 0x00000001);
}

TEST_CASE("account create-account result codes match legacy",
          "[protocol][bnet][account_wire_types]")
{
    REQUIRE(a::kCreateAccountW3ResultOk           == 0x00000000);
    REQUIRE(a::kCreateAccountW3ResultExist        == 0x00000004);
    REQUIRE(a::kCreateAccountW3ResultEmpty        == 0x00000007);
    REQUIRE(a::kCreateAccountW3ResultInvalid      == 0x00000008);
    REQUIRE(a::kCreateAccountW3ResultBanned       == 0x00000009);
    REQUIRE(a::kCreateAccountW3ResultShort        == 0x0000000A);
    REQUIRE(a::kCreateAccountW3ResultPunctuation  == 0x0000000B);
    REQUIRE(a::kCreateAccountW3ResultPunctuation2 == 0x0000000C);

    REQUIRE(a::kCreateAcctReply2ResultOk                   == 0x00000000);
    REQUIRE(a::kCreateAcctReply2ResultShort                == 0x00000001);
    REQUIRE(a::kCreateAcctReply2ResultInvalid              == 0x00000002);
    REQUIRE(a::kCreateAcctReply2ResultBanned               == 0x00000003);
    REQUIRE(a::kCreateAcctReply2ResultExist                == 0x00000004);
    REQUIRE(a::kCreateAcctReply2ResultLastCreateInProgress == 0x00000005);
    REQUIRE(a::kCreateAcctReply2ResultAlphanum             == 0x00000006);
    REQUIRE(a::kCreateAcctReply2ResultPunctuation          == 0x00000007);
    REQUIRE(a::kCreateAcctReply2ResultPunctuation2         == 0x00000008);
}
