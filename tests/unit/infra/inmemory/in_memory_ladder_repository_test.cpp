// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "infra/inmemory/ladder_repository.hpp"

namespace pvpgn::infra::inmemory {

TEST_CASE("InMemoryLadderRepository: GetRankNotFound", "[infra][inmemory]") {
    InMemoryLadderRepository repository;
    auto result = repository.get_rank(domain::AccountId{999});
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("InMemoryLadderRepository: GetTopNEmpty", "[infra][inmemory]") {
    InMemoryLadderRepository repository;
    auto result = repository.get_top_n(10);
    REQUIRE(result.has_value());
    REQUIRE(result.value().size() == 0);
}

}  // namespace pvpgn::infra::inmemory
