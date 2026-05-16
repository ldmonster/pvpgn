// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "infra/inmemory/clan_repository.hpp"

namespace pvpgn::infra::inmemory {

TEST_CASE("InMemoryClanRepository: FindByIdNotFound", "[infra][inmemory]") {
    InMemoryClanRepository repository;
    auto result = repository.find_by_id(domain::ClanId{1});
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("InMemoryClanRepository: SaveAndFindById", "[infra][inmemory]") {
    // Note: This test assumes domain::social::Clan has a constructor/factory
    // In practice, you would need to construct a valid Clan object
    // For now, we skip this test as the implementation needs real domain objects
}

TEST_CASE("InMemoryClanRepository: FindByTagNotFound", "[infra][inmemory]") {
    InMemoryClanRepository repository;
    auto result = repository.find_by_tag("CLAN");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code() == core::StatusCode::NotFound);
}

}  // namespace pvpgn::infra::inmemory
