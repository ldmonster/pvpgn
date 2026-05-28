// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "infra/inmemory/in_memory_team_repository.hpp"

namespace pvpgn::infra::inmemory {

TEST_CASE("InMemoryTeamRepository: FindByIdNotFound", "[infra][inmemory]") {
    InMemoryTeamRepository repo;
    auto result = repo.find_by_id(domain::TeamId{42});
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("InMemoryTeamRepository: FindByIdUnknownId", "[infra][inmemory]") {
    InMemoryTeamRepository repo;
    auto result = repo.find_by_id(domain::TeamId{0});
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("InMemoryTeamRepository: FindByMemberEmptyRepo", "[infra][inmemory]") {
    InMemoryTeamRepository repo;
    auto result = repo.find_by_member(domain::AccountId{1});
    REQUIRE(result.has_value());
    REQUIRE(result.value().empty());
}

TEST_CASE("InMemoryTeamRepository: RemoveNotFound", "[infra][inmemory]") {
    InMemoryTeamRepository repo;
    auto result = repo.remove(domain::TeamId{99});
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("InMemoryTeamRepository: RemoveUnknownIdReturnsNotFound", "[infra][inmemory]") {
    InMemoryTeamRepository repo;
    auto r1 = repo.remove(domain::TeamId{1});
    REQUIRE_FALSE(r1.has_value());
    auto r2 = repo.remove(domain::TeamId{1000});
    REQUIRE_FALSE(r2.has_value());
}

}  // namespace pvpgn::infra::inmemory
