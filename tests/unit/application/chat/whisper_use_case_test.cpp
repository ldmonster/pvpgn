// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "application/chat/whisper_use_case.hpp"

using namespace pvpgn::application::chat;

namespace {

WhisperRequest make(std::string_view sender,
                    std::string_view target,
                    std::string_view body,
                    WhisperTarget state = {true, false, false}) {
    return WhisperRequest{sender, target, body, state};
}

}  // namespace

TEST_CASE("whisper: empty target -> NoTarget",
          "[application][chat][whisper]") {
    REQUIRE(decide_whisper(make("alice", "", "hi")) == WhisperVerdict::NoTarget);
}

TEST_CASE("whisper: blank body -> EmptyBody",
          "[application][chat][whisper]") {
    REQUIRE(decide_whisper(make("alice", "bob", ""))
            == WhisperVerdict::EmptyBody);
    REQUIRE(decide_whisper(make("alice", "bob", "  \t  "))
            == WhisperVerdict::EmptyBody);
}

TEST_CASE("whisper: self-target case-insensitive -> SelfWhisper",
          "[application][chat][whisper]") {
    REQUIRE(decide_whisper(make("Alice", "alice", "hi"))
            == WhisperVerdict::SelfWhisper);
    REQUIRE(decide_whisper(make("ALICE", "Alice", "hi"))
            == WhisperVerdict::SelfWhisper);
}

TEST_CASE("whisper: target offline -> TargetOffline",
          "[application][chat][whisper]") {
    auto r = make("alice", "bob", "hi", {false, false, false});
    REQUIRE(decide_whisper(r) == WhisperVerdict::TargetOffline);
}

TEST_CASE("whisper: target DnD -> TargetDnd",
          "[application][chat][whisper]") {
    auto r = make("alice", "bob", "hi", {true, true, false});
    REQUIRE(decide_whisper(r) == WhisperVerdict::TargetDnd);
}

TEST_CASE("whisper: target ignores sender -> IgnoredByTarget",
          "[application][chat][whisper]") {
    auto r = make("alice", "bob", "hi", {true, false, true});
    REQUIRE(decide_whisper(r) == WhisperVerdict::IgnoredByTarget);
}

TEST_CASE("whisper: happy path -> Delivered",
          "[application][chat][whisper]") {
    REQUIRE(decide_whisper(make("alice", "bob", "hi"))
            == WhisperVerdict::Delivered);
}

TEST_CASE("whisper: rejection priority - NoTarget beats EmptyBody",
          "[application][chat][whisper][priority]") {
    auto r = make("alice", "", "");
    REQUIRE(decide_whisper(r) == WhisperVerdict::NoTarget);
}

TEST_CASE("whisper: rejection priority - EmptyBody beats SelfWhisper",
          "[application][chat][whisper][priority]") {
    auto r = make("alice", "alice", "  ");
    REQUIRE(decide_whisper(r) == WhisperVerdict::EmptyBody);
}
