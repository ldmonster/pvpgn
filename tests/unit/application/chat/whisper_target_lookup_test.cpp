// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "application/chat/whisper_target_lookup.hpp"
#include "application/chat/whisper_use_case.hpp"

using namespace pvpgn::application::chat;

TEST_CASE("whisper lookup: NullWhisperTargetLookup reports offline",
          "[application][chat][whisper][lookup]") {
    NullWhisperTargetLookup lk;
    auto s = lk.lookup("alice", "bob");
    REQUIRE_FALSE(s.online);
    REQUIRE_FALSE(s.dnd);
    REQUIRE_FALSE(s.ignored_by);
}

TEST_CASE("whisper lookup: MapWhisperTargetLookup case-insensitive hits",
          "[application][chat][whisper][lookup]") {
    MapWhisperTargetLookup lk;
    lk.set("Bob", WhisperTarget{true, true, false});

    auto exact = lk.lookup("alice", "Bob");
    REQUIRE(exact.online);
    REQUIRE(exact.dnd);

    auto lower = lk.lookup("alice", "bob");
    REQUIRE(lower.online);
    REQUIRE(lower.dnd);

    auto upper = lk.lookup("alice", "BOB");
    REQUIRE(upper.online);
    REQUIRE(upper.dnd);
}

TEST_CASE("whisper lookup: MapWhisperTargetLookup miss returns offline",
          "[application][chat][whisper][lookup]") {
    MapWhisperTargetLookup lk;
    lk.set("bob", WhisperTarget{true, false, false});
    auto miss = lk.lookup("alice", "charlie");
    REQUIRE_FALSE(miss.online);
}

TEST_CASE("whisper lookup: feeds into decide_whisper",
          "[application][chat][whisper][lookup][integration]") {
    MapWhisperTargetLookup lk;
    lk.set("bob", WhisperTarget{true, false, false});       // online
    lk.set("dave", WhisperTarget{true, true, false});       // dnd
    lk.set("eve",  WhisperTarget{true, false, true});       // ignores sender

    auto check = [&](std::string_view target,
                     WhisperVerdict expected) {
        WhisperRequest req{
            "alice", target, "hi",
            lk.lookup("alice", target)
        };
        REQUIRE(decide_whisper(req) == expected);
    };

    check("bob",     WhisperVerdict::Delivered);
    check("dave",    WhisperVerdict::TargetDnd);
    check("eve",     WhisperVerdict::IgnoredByTarget);
    check("charlie", WhisperVerdict::TargetOffline);
}
