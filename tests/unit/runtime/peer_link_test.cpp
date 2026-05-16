// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "runtime/peer_link.hpp"

using namespace pvpgn::runtime;

TEST_CASE("PeerMessage default construction", "[runtime][peer_link]") {
    PeerMessage msg;
    REQUIRE(msg.type.empty());
    REQUIRE(msg.payload.empty());
    REQUIRE(msg.request_id.empty());
    REQUIRE(msg.timestamp.empty());
    REQUIRE(msg.token.empty());
}

TEST_CASE("PeerMessage field assignment", "[runtime][peer_link]") {
    PeerMessage msg;
    msg.type = "character.lock";
    msg.payload = R"({"character_name": "Sorceress"})";
    msg.request_id = "req-123";
    msg.timestamp = "2026-05-15T13:18:00Z";
    msg.token = "eyJhbGc...";
    
    REQUIRE(msg.type == "character.lock");
    REQUIRE(msg.payload == R"({"character_name": "Sorceress"})");
    REQUIRE(msg.request_id == "req-123");
    REQUIRE(msg.timestamp == "2026-05-15T13:18:00Z");
    REQUIRE(msg.token == "eyJhbGc...");
}

TEST_CASE("CapabilityToken default construction", "[runtime][peer_link]") {
    CapabilityToken token;
    REQUIRE(token.issuer.empty());
    REQUIRE(token.subject.empty());
    REQUIRE(token.capabilities.empty());
    REQUIRE(token.expiration == 0);
}

TEST_CASE("CapabilityToken field assignment", "[runtime][peer_link]") {
    CapabilityToken token;
    token.issuer = "d2cs";
    token.subject = "d2dbs";
    token.capabilities.push_back("character.read");
    token.capabilities.push_back("character.write");
    token.expiration = 1684156680;
    
    REQUIRE(token.issuer == "d2cs");
    REQUIRE(token.subject == "d2dbs");
    REQUIRE(token.capabilities.size() == 2);
    REQUIRE(token.capabilities[0] == "character.read");
    REQUIRE(token.capabilities[1] == "character.write");
    REQUIRE(token.expiration == 1684156680);
}

TEST_CASE("CapabilityToken encode returns non-empty string", "[runtime][peer_link]") {
    CapabilityToken token;
    token.issuer = "d2cs";
    token.subject = "d2dbs";
    token.capabilities.push_back("character.read");
    token.expiration = 1684156680;
    
    auto encoded = token.encode();
    REQUIRE_FALSE(encoded.empty());
}

TEST_CASE("CapabilityToken decode from encoded token", "[runtime][peer_link]") {
    CapabilityToken original;
    original.issuer = "d2cs";
    original.subject = "d2dbs";
    original.capabilities.push_back("character.read");
    original.expiration = 1684156680;
    
    auto encoded = original.encode();
    auto decoded = CapabilityToken::decode(encoded);
    
    REQUIRE(decoded);
    REQUIRE(decoded.value().issuer == "d2cs");
    REQUIRE(decoded.value().subject == "d2dbs");
    REQUIRE(decoded.value().expiration == 1684156680);
}

TEST_CASE("CapabilityToken decode invalid token returns error", "[runtime][peer_link]") {
    auto result = CapabilityToken::decode("invalid.token.format");
    REQUIRE_FALSE(result);
}

TEST_CASE("CapabilityToken multiple capabilities", "[runtime][peer_link]") {
    CapabilityToken token;
    token.issuer = "d2cs";
    token.subject = "d2dbs";
    token.capabilities.push_back("character.read");
    token.capabilities.push_back("character.write");
    token.capabilities.push_back("account.read");
    token.expiration = 1684156680;
    
    REQUIRE(token.capabilities.size() == 3);
    REQUIRE(token.capabilities[0] == "character.read");
    REQUIRE(token.capabilities[1] == "character.write");
    REQUIRE(token.capabilities[2] == "account.read");
}
