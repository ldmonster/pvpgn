// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>
#include <string_view>

#include "protocol/irc/wire_types.hpp"

namespace w = pvpgn::protocol::irc::wire;

TEST_CASE("irc::wire structural constants match legacy",
          "[protocol][irc][wire_types]")
{
    REQUIRE(w::kWolNicknameLen == 9);
    REQUIRE(std::string_view(w::kChannelPrefix) == "(ohv)@%+");
    REQUIRE(std::string_view(w::kChannelType) == "#");
}

TEST_CASE("irc::wire RPL_* core replies match legacy numerics",
          "[protocol][irc][wire_types]")
{
    REQUIRE(w::kRplWelcome        == 1);
    REQUIRE(w::kRplMyInfo         == 4);
    REQUIRE(w::kRplISupport       == 5);
    REQUIRE(w::kRplTraceLink      == 200);
    REQUIRE(w::kRplStatsUptime    == 242);
    REQUIRE(w::kRplLUserMe        == 255);
    REQUIRE(w::kRplWhoisUser      == 311);
    REQUIRE(w::kRplEndOfWhois     == 318);
    REQUIRE(w::kRplListEnd        == 323);
    REQUIRE(w::kRplTopic          == 332);
    REQUIRE(w::kRplNamReply       == 353);
    REQUIRE(w::kRplEndOfNames     == 366);
    REQUIRE(w::kRplMotd           == 372);
    REQUIRE(w::kRplEndOfMotd      == 376);
    REQUIRE(w::kRplTime           == 391);
}

TEST_CASE("irc::wire WOL replies match legacy numerics",
          "[protocol][irc][wire_types]")
{
    REQUIRE(w::kRplGetLocale       == 309);
    REQUIRE(w::kRplSetLocale       == 310);
    REQUIRE(w::kRplGameChannel     == 326);
    REQUIRE(w::kRplGetCodepage     == 328);
    REQUIRE(w::kRplSetCodepage     == 329);
    REQUIRE(w::kRplGetBuddy        == 333);
    REQUIRE(w::kRplBattleClan      == 358);
    REQUIRE(w::kRplAnnounce        == 377);
    REQUIRE(w::kRplBadLogin        == 378);
    REQUIRE(w::kRplVerchkNonreq    == 379);
    REQUIRE(w::kRplFindUserEx      == 398);
    REQUIRE(w::kRplGetInsider      == 399);
    REQUIRE(w::kRplWolServ         == 605);
    REQUIRE(w::kRplGameresServ     == 608);
    REQUIRE(w::kRplLadderServ      == 609);
    REQUIRE(w::kRplPingServer      == 615);
    REQUIRE(w::kErrIdNoExist       == 439);
    REQUIRE(w::kErrGameHasClosed   == 478);
}

TEST_CASE("irc::wire ERR_* core errors match legacy numerics",
          "[protocol][irc][wire_types]")
{
    REQUIRE(w::kErrFirstError       == 400);
    REQUIRE(w::kErrNoSuchNick       == 401);
    REQUIRE(w::kErrNicknameInUse    == 433);
    REQUIRE(w::kErrNickCollision    == 436);
    REQUIRE(w::kErrNotRegistered    == 451);
    REQUIRE(w::kErrNeedMoreParams   == 461);
    REQUIRE(w::kErrPasswdMismatch   == 464);
    REQUIRE(w::kErrChannelIsFull    == 471);
    REQUIRE(w::kErrBannedFromChan   == 474);
    REQUIRE(w::kErrBadChannelKey    == 475);
    REQUIRE(w::kErrNoPrivileges     == 481);
    REQUIRE(w::kErrChanOPrivsNeeded == 482);
    REQUIRE(w::kErrCantKillServer   == 483);
    REQUIRE(w::kErrUsersDontMatch   == 502);
    REQUIRE(w::kErrLastError        == 521);
}
