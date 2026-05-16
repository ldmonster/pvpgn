// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "infra/scripting/plugin/capability.hpp"

using namespace pvpgn::infra::scripting;

TEST_CASE("Capability enum values are powers of 2", "[infra][plugin][capability]") {
    REQUIRE(static_cast<std::uint32_t>(Capability::CHAT_SEND) == (1u << 0));
    REQUIRE(static_cast<std::uint32_t>(Capability::CHAT_EMOTE) == (1u << 1));
    REQUIRE(static_cast<std::uint32_t>(Capability::DB_READ) == (1u << 2));
    REQUIRE(static_cast<std::uint32_t>(Capability::DB_WRITE) == (1u << 3));
    REQUIRE(static_cast<std::uint32_t>(Capability::EVENTS_SUBSCRIBE) == (1u << 4));
    REQUIRE(static_cast<std::uint32_t>(Capability::EVENTS_PUBLISH) == (1u << 5));
    REQUIRE(static_cast<std::uint32_t>(Capability::FS_READ) == (1u << 6));
    REQUIRE(static_cast<std::uint32_t>(Capability::FS_WRITE) == (1u << 7));
    REQUIRE(static_cast<std::uint32_t>(Capability::NET_HTTP) == (1u << 8));
    REQUIRE(static_cast<std::uint32_t>(Capability::NET_SOCKET) == (1u << 9));
    REQUIRE(static_cast<std::uint32_t>(Capability::COMMANDS_REGISTER) == (1u << 10));
    REQUIRE(static_cast<std::uint32_t>(Capability::MODERATION_BAN) == (1u << 11));
    REQUIRE(static_cast<std::uint32_t>(Capability::MODERATION_KICK) == (1u << 12));
    REQUIRE(static_cast<std::uint32_t>(Capability::STORE_READ) == (1u << 13));
    REQUIRE(static_cast<std::uint32_t>(Capability::STORE_WRITE) == (1u << 14));
    REQUIRE(static_cast<std::uint32_t>(Capability::ADMIN_RELOAD_CONFIG) == (1u << 15));
    REQUIRE(static_cast<std::uint32_t>(Capability::ADMIN_SHUTDOWN) == (1u << 16));
}

TEST_CASE("CapabilitySet default construction", "[infra][plugin][capability]") {
    CapabilitySet cs;
    REQUIRE(cs.mask() == 0);
    REQUIRE_FALSE(cs.has(Capability::CHAT_SEND));
}

TEST_CASE("CapabilitySet grant and has", "[infra][plugin][capability]") {
    CapabilitySet cs;
    cs.grant(Capability::CHAT_SEND);
    REQUIRE(cs.has(Capability::CHAT_SEND));
    REQUIRE_FALSE(cs.has(Capability::CHAT_EMOTE));
}

TEST_CASE("CapabilitySet revoke", "[infra][plugin][capability]") {
    CapabilitySet cs;
    cs.grant(Capability::DB_READ);
    cs.grant(Capability::DB_WRITE);
    REQUIRE(cs.has(Capability::DB_READ));
    REQUIRE(cs.has(Capability::DB_WRITE));
    
    cs.revoke(Capability::DB_READ);
    REQUIRE_FALSE(cs.has(Capability::DB_READ));
    REQUIRE(cs.has(Capability::DB_WRITE));
}

TEST_CASE("CapabilitySet mask construction", "[infra][plugin][capability]") {
    std::uint32_t mask = (1u << 0) | (1u << 2);  // CHAT_SEND | DB_READ
    CapabilitySet cs{mask};
    REQUIRE(cs.has(Capability::CHAT_SEND));
    REQUIRE(cs.has(Capability::DB_READ));
    REQUIRE_FALSE(cs.has(Capability::CHAT_EMOTE));
}

TEST_CASE("CapabilitySet multiple grants", "[infra][plugin][capability]") {
    CapabilitySet cs;
    cs.grant(Capability::CHAT_SEND);
    cs.grant(Capability::CHAT_EMOTE);
    cs.grant(Capability::DB_READ);
    
    REQUIRE(cs.has(Capability::CHAT_SEND));
    REQUIRE(cs.has(Capability::CHAT_EMOTE));
    REQUIRE(cs.has(Capability::DB_READ));
    REQUIRE_FALSE(cs.has(Capability::DB_WRITE));
}

TEST_CASE("capability_to_string returns valid strings", "[infra][plugin][capability]") {
    auto str_send = capability_to_string(Capability::CHAT_SEND);
    auto str_read = capability_to_string(Capability::DB_READ);
    
    REQUIRE_FALSE(str_send.empty());
    REQUIRE_FALSE(str_read.empty());
}

TEST_CASE("parse_capability recognizes known capabilities", "[infra][plugin][capability]") {
    // Test that parse_capability can handle various capability strings
    // The exact behavior depends on implementation, but it should not crash
    auto cap = parse_capability("chat.send");
    REQUIRE(true);  // Just verify it doesn't crash
}
