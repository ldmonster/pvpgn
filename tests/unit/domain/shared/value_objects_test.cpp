// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>
#include "domain/shared/ids.hpp"
#include "domain/chat/channel_name.hpp"
#include "domain/realm/realm_name.hpp"
#include "domain/social/clan_tag.hpp"
#include "domain/gameplay/game_name.hpp"

using namespace pvpgn::domain;

// --- ConnectionId ---
TEST_CASE("ConnectionId is a strong ID", "[domain][ids]") {
    ConnectionId a{1}, b{2}, c{1};
    REQUIRE(a != b);
    REQUIRE(a == c);
    REQUIRE(a.value() == 1u);
    REQUIRE(a.is_valid());
    REQUIRE(!ConnectionId{0}.is_valid());
}

// --- ChannelName ---
TEST_CASE("ChannelName parse accepts valid names", "[domain][chat]") {
    REQUIRE(chat::ChannelName::parse("Diablo II").has_value());
    REQUIRE(chat::ChannelName::parse("a").has_value());
    std::string max64(64, 'x');
    REQUIRE(chat::ChannelName::parse(max64).has_value());
}

TEST_CASE("ChannelName parse rejects invalid names", "[domain][chat]") {
    REQUIRE(!chat::ChannelName::parse("").has_value());
    std::string too_long(65, 'x');
    REQUIRE(!chat::ChannelName::parse(too_long).has_value());
    REQUIRE(!chat::ChannelName::parse("bad\x01char").has_value());
    REQUIRE(!chat::ChannelName::parse("tab\there").has_value());
}

TEST_CASE("ChannelName value round-trips", "[domain][chat]") {
    auto name = chat::ChannelName::parse("Warcraft III");
    REQUIRE(name.has_value());
    REQUIRE(name->value() == "Warcraft III");
    REQUIRE(name->str() == "Warcraft III");
}

// --- RealmName ---
TEST_CASE("RealmName parse accepts valid names", "[domain][realm]") {
    REQUIRE(realm::RealmName::parse("USEast").has_value());
    REQUIRE(realm::RealmName::parse("a").has_value());
    std::string max32(32, 'r');
    REQUIRE(realm::RealmName::parse(max32).has_value());
}

TEST_CASE("RealmName parse rejects invalid names", "[domain][realm]") {
    REQUIRE(!realm::RealmName::parse("").has_value());
    std::string too_long(33, 'r');
    REQUIRE(!realm::RealmName::parse(too_long).has_value());
    REQUIRE(!realm::RealmName::parse("bad\nline").has_value());
}

// --- ClanTag ---
TEST_CASE("ClanTag parse accepts valid tags", "[domain][social]") {
    REQUIRE(social::ClanTag::parse("AB").has_value());
    REQUIRE(social::ClanTag::parse("ABCD").has_value());
    REQUIRE(social::ClanTag::parse("ABC").has_value());
}

TEST_CASE("ClanTag parse rejects invalid tags", "[domain][social]") {
    REQUIRE(!social::ClanTag::parse("").has_value());
    REQUIRE(!social::ClanTag::parse("A").has_value());
    REQUIRE(!social::ClanTag::parse("ABCDE").has_value());
    REQUIRE(!social::ClanTag::parse("A\x01").has_value());
}

// --- GameName ---
TEST_CASE("GameName parse accepts valid names", "[domain][gameplay]") {
    REQUIRE(gameplay::GameName::parse("My Game").has_value());
    REQUIRE(gameplay::GameName::parse("g").has_value());
    std::string max64(64, 'g');
    REQUIRE(gameplay::GameName::parse(max64).has_value());
}

TEST_CASE("GameName parse rejects invalid names", "[domain][gameplay]") {
    REQUIRE(!gameplay::GameName::parse("").has_value());
    std::string too_long(65, 'g');
    REQUIRE(!gameplay::GameName::parse(too_long).has_value());
    REQUIRE(!gameplay::GameName::parse(std::string("bad\x00""char", 8)).has_value());
}
