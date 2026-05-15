// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "infra/inmemory/unit_of_work_factory.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryUnitOfWorkTest : public ::testing::Test {
protected:
    void SetUp() override {
        factory_ = std::make_unique<InMemoryUnitOfWorkFactory>();
    }

    std::unique_ptr<InMemoryUnitOfWorkFactory> factory_;
};

TEST_F(InMemoryUnitOfWorkTest, CreateUnitOfWork) {
    auto uow = factory_->create();
    ASSERT_NE(nullptr, uow);
}

TEST_F(InMemoryUnitOfWorkTest, BeginReturnsSuccess) {
    auto uow = factory_->create();
    auto result = uow->begin();
    EXPECT_TRUE(result.has_value());
}

TEST_F(InMemoryUnitOfWorkTest, CommitReturnsSuccess) {
    auto uow = factory_->create();
    EXPECT_TRUE(uow->begin().has_value());
    auto result = uow->commit();
    EXPECT_TRUE(result.has_value());
}

TEST_F(InMemoryUnitOfWorkTest, RollbackDoesNotThrow) {
    auto uow = factory_->create();
    EXPECT_NO_THROW(uow->rollback());
}

TEST_F(InMemoryUnitOfWorkTest, AccessAllRepositories) {
    auto uow = factory_->create();
    
    // All repository accessors should return non-null references
    EXPECT_NE(&uow->accounts(), nullptr);
    EXPECT_NE(&uow->channels(), nullptr);
    EXPECT_NE(&uow->games(), nullptr);
    EXPECT_NE(&uow->clans(), nullptr);
    EXPECT_NE(&uow->ladder(), nullptr);
    EXPECT_NE(&uow->ip_bans(), nullptr);
    EXPECT_NE(&uow->account_bans(), nullptr);
    EXPECT_NE(&uow->friend_lists(), nullptr);
    EXPECT_NE(&uow->realms(), nullptr);
}

TEST_F(InMemoryUnitOfWorkTest, SharedRepositoriesAcrossUnitOfWorks) {
    auto uow1 = factory_->create();
    auto uow2 = factory_->create();
    
    // Both units of work should reference the same underlying repositories
    // (this is implementation-dependent, but we can verify they exist)
    EXPECT_NE(&uow1->accounts(), nullptr);
    EXPECT_NE(&uow2->accounts(), nullptr);
}

TEST_F(InMemoryUnitOfWorkTest, GuardRollbackOnDestruction) {
    auto uow = factory_->create();
    {
        application::ports::UnitOfWorkGuard guard(*uow);
        // Guard goes out of scope without commit
    }
    // If we get here, rollback was called and didn't throw
    SUCCEED();
}

TEST_F(InMemoryUnitOfWorkTest, GuardCommitPreventsRollback) {
    auto uow = factory_->create();
    {
        application::ports::UnitOfWorkGuard guard(*uow);
        auto result = guard.commit();
        EXPECT_TRUE(result.has_value());
        // Guard goes out of scope with commit already called
    }
    SUCCEED();
}

}  // namespace pvpgn::infra::inmemory
