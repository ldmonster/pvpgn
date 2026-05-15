// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "infra/inmemory/ladder_repository.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryLadderRepositoryTest : public ::testing::Test {
protected:
    InMemoryLadderRepository repository_;
};

TEST_F(InMemoryLadderRepositoryTest, FindByAccountNotFound) {
    auto result = repository_.find_by_account(domain::AccountId{1});
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().status(), core::StatusCode::NotFound);
}

TEST_F(InMemoryLadderRepositoryTest, TotalEntriesEmpty) {
    auto count = repository_.total_entries(domain::ClientTag::Starcraft);
    EXPECT_EQ(0, count);
}

TEST_F(InMemoryLadderRepositoryTest, GetPageEmpty) {
    auto result = repository_.get_page(domain::ClientTag::Starcraft, 0, 10);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(0, result.value().size());
}

TEST_F(InMemoryLadderRepositoryTest, GetPageOffsetBeyondSize) {
    auto result = repository_.get_page(domain::ClientTag::Starcraft, 100, 10);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(0, result.value().size());
}

TEST_F(InMemoryLadderRepositoryTest, ForEachEmpty) {
    int count = 0;
    repository_.for_each(domain::ClientTag::Starcraft, [&count](const auto&) {
        count++;
        return true;
    });
    EXPECT_EQ(0, count);
}

}  // namespace pvpgn::infra::inmemory
