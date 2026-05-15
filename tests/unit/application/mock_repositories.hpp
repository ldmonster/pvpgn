// SPDX-License-Identifier: GPL-2.0-or-later
//
// Reusable test fixtures and mock repositories for application layer tests.
// Uses in-memory implementations from infra/ for consistent behavior across all tests.

#pragma once

#include <memory>

#include "infra/inmemory/account_repository.hpp"
#include "infra/storage/repository/channel_repository.hpp"
#include "infra/storage/repository/game_repository.hpp"

namespace pvpgn::tests {

/// Fixture providing all in-memory repositories for application layer tests.
struct MockRepositories {
    infra::inmemory::InMemoryAccountRepository accounts;
    infra::storage::InMemoryChannelRepository channels;
    infra::storage::InMemoryGameRepository games;

    MockRepositories() = default;

    // Delete copy/move to keep fixtures stable
    MockRepositories(const MockRepositories&) = delete;
    MockRepositories& operator=(const MockRepositories&) = delete;
    MockRepositories(MockRepositories&&) = delete;
    MockRepositories& operator=(MockRepositories&&) = delete;
};

}  // namespace pvpgn::tests
