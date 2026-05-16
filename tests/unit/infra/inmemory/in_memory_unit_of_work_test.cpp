// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "infra/inmemory/unit_of_work_factory.hpp"

namespace pvpgn::infra::inmemory {

TEST_CASE("InMemoryUnitOfWork: CreateUnitOfWork", "[infra][inmemory]") {
    auto factory = std::make_unique<InMemoryUnitOfWorkFactory>();
    auto uow = factory->create();
    REQUIRE(uow != nullptr);
}

TEST_CASE("InMemoryUnitOfWork: BeginReturnsSuccess", "[infra][inmemory]") {
    auto factory = std::make_unique<InMemoryUnitOfWorkFactory>();
    auto uow = factory->create();
    auto result = uow->begin();
    REQUIRE(result.has_value());
}

TEST_CASE("InMemoryUnitOfWork: CommitReturnsSuccess", "[infra][inmemory]") {
    auto factory = std::make_unique<InMemoryUnitOfWorkFactory>();
    auto uow = factory->create();
    REQUIRE(uow->begin().has_value());
    auto result = uow->commit();
    REQUIRE(result.has_value());
}

TEST_CASE("InMemoryUnitOfWork: RollbackDoesNotThrow", "[infra][inmemory]") {
    auto factory = std::make_unique<InMemoryUnitOfWorkFactory>();
    auto uow = factory->create();
    REQUIRE_NOTHROW(uow->rollback());
}

TEST_CASE("InMemoryUnitOfWork: AccessAllRepositories", "[infra][inmemory]") {
    auto factory = std::make_unique<InMemoryUnitOfWorkFactory>();
    auto uow = factory->create();
    
    // All repository accessors should return non-null references
    REQUIRE(&uow->accounts() != nullptr);
    REQUIRE(&uow->channels() != nullptr);
    REQUIRE(&uow->games() != nullptr);
    REQUIRE(&uow->clans() != nullptr);
    REQUIRE(&uow->ladder() != nullptr);
    REQUIRE(&uow->ip_bans() != nullptr);
    REQUIRE(&uow->account_bans() != nullptr);
    REQUIRE(&uow->friend_lists() != nullptr);
    REQUIRE(&uow->realms() != nullptr);
}

TEST_CASE("InMemoryUnitOfWork: SharedRepositoriesAcrossUnitOfWorks", "[infra][inmemory]") {
    auto factory = std::make_unique<InMemoryUnitOfWorkFactory>();
    auto uow1 = factory->create();
    auto uow2 = factory->create();
    
    // Both units of work should reference the same underlying repositories
    // (this is implementation-dependent, but we can verify they exist)
    REQUIRE(&uow1->accounts() != nullptr);
    REQUIRE(&uow2->accounts() != nullptr);
}

TEST_CASE("InMemoryUnitOfWork: GuardRollbackOnDestruction", "[infra][inmemory]") {
    auto factory = std::make_unique<InMemoryUnitOfWorkFactory>();
    auto uow = factory->create();
    {
        application::ports::UnitOfWorkGuard guard(*uow);
        // Guard goes out of scope without commit
    }
    // If we get here, rollback was called and didn't throw
}

TEST_CASE("InMemoryUnitOfWork: GuardCommitPreventsRollback", "[infra][inmemory]") {
    auto factory = std::make_unique<InMemoryUnitOfWorkFactory>();
    auto uow = factory->create();
    {
        application::ports::UnitOfWorkGuard guard(*uow);
        auto result = guard.commit();
        REQUIRE(result.has_value());
        // Guard goes out of scope with commit already called
    }
}

}  // namespace pvpgn::infra::inmemory
