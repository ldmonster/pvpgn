// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "infra/inmemory/in_memory_session_token_issuer.hpp"

namespace pvpgn::infra::inmemory {

TEST_CASE("InMemorySessionTokenIssuer: IssueReturnsNonEmptyToken", "[infra][inmemory]") {
    InMemorySessionTokenIssuer issuer;
    auto token = issuer.issue(domain::AccountId{1});
    REQUIRE_FALSE(token.empty());
}

TEST_CASE("InMemorySessionTokenIssuer: IssueReturnsUniqueTokensPerCall", "[infra][inmemory]") {
    InMemorySessionTokenIssuer issuer;
    auto t1 = issuer.issue(domain::AccountId{1});
    auto t2 = issuer.issue(domain::AccountId{1});
    auto t3 = issuer.issue(domain::AccountId{2});
    REQUIRE(t1 != t2);
    REQUIRE(t1 != t3);
    REQUIRE(t2 != t3);
}

TEST_CASE("InMemorySessionTokenIssuer: ValidateIssuedToken", "[infra][inmemory]") {
    InMemorySessionTokenIssuer issuer;
    const domain::AccountId account{42};
    auto token = issuer.issue(account);

    auto result = issuer.validate(token);
    REQUIRE(result.has_value());
    REQUIRE(result.value().value() == account.value());
}

TEST_CASE("InMemorySessionTokenIssuer: ValidateUnknownTokenReturnsNotFound", "[infra][inmemory]") {
    InMemorySessionTokenIssuer issuer;
    auto result = issuer.validate("tok-9999-bogus");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("InMemorySessionTokenIssuer: RevokeInvalidatesToken", "[infra][inmemory]") {
    InMemorySessionTokenIssuer issuer;
    auto token = issuer.issue(domain::AccountId{7});

    issuer.revoke(token);

    auto result = issuer.validate(token);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("InMemorySessionTokenIssuer: RevokeUnknownTokenIsNoOp", "[infra][inmemory]") {
    InMemorySessionTokenIssuer issuer;
    REQUIRE_NOTHROW(issuer.revoke("nonexistent-token"));
}

TEST_CASE("InMemorySessionTokenIssuer: RevokeDoesNotAffectOtherTokens", "[infra][inmemory]") {
    InMemorySessionTokenIssuer issuer;
    auto t1 = issuer.issue(domain::AccountId{10});
    auto t2 = issuer.issue(domain::AccountId{20});

    issuer.revoke(t1);

    REQUIRE_FALSE(issuer.validate(t1).has_value());
    REQUIRE(issuer.validate(t2).has_value());
    REQUIRE(issuer.validate(t2).value().value() == 20u);
}

TEST_CASE("InMemorySessionTokenIssuer: MultipleTokensForSameAccount", "[infra][inmemory]") {
    InMemorySessionTokenIssuer issuer;
    const domain::AccountId account{5};

    auto t1 = issuer.issue(account);
    auto t2 = issuer.issue(account);

    REQUIRE(issuer.validate(t1).value().value() == account.value());
    REQUIRE(issuer.validate(t2).value().value() == account.value());

    issuer.revoke(t1);
    REQUIRE_FALSE(issuer.validate(t1).has_value());
    REQUIRE(issuer.validate(t2).has_value());
}

}  // namespace pvpgn::infra::inmemory
