// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::ladder::GetLadderPage`.
// Uses inline fakes for ILadderRepository and IAccountRepository.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <unordered_map>
#include <vector>

#include "application/ladder/get_ladder_page.hpp"
#include "application/ports/account_repository.hpp"
#include "application/ports/ladder_repository.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/identity/account.hpp"
#include "domain/ladder/ladder.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"

namespace {

using namespace pvpgn;
using application::ladder::GetLadderPage;
using application::ladder::GetLadderPageQuery;
using application::ladder::LadderPage;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

domain::identity::Account make_account(domain::AccountId id,
                                       std::string_view name) {
    auto uname = domain::UserName::parse(name);
    REQUIRE(uname);
    auto locale = domain::Locale::parse_or_default("enUS");
    return domain::identity::Account::rehydrate(
        id, uname.value(), domain::BNHash{}, locale,
        domain::identity::CommandGroupMask{}, std::nullopt, false);
}

// ---------------------------------------------------------------------------
// Inline fake: ILadderRepository
// ---------------------------------------------------------------------------

class FakeLadderRepository final : public application::ports::ILadderRepository {
public:
    std::vector<domain::ladder::LadderEntry> entries;

    core::Result<uint32_t, core::Error>
    get_rank(std::string_view /*account_name*/) override {
        if (entries.empty()) {
            return core::fail(core::Error{core::StatusCode::NotFound, "no entries"});
        }
        return 1u;
    }

    core::Result<void, core::Error>
    save_entry(const domain::ladder::LadderEntry& entry) override {
        for (auto& e : entries) {
            if (e.account == entry.account) { e = entry; return core::ok(); }
        }
        entries.push_back(entry);
        return core::ok();
    }

    core::Result<std::vector<domain::ladder::LadderEntry>, core::Error>
    get_top_n(uint32_t n) override {
        auto copy = entries;
        if (copy.size() > n) copy.resize(n);
        return copy;
    }
};

// ---------------------------------------------------------------------------
// Inline fake: IAccountRepository
// ---------------------------------------------------------------------------

class FakeAccountRepository final : public application::ports::IAccountRepository {
public:
    void add(domain::identity::Account acct) {
        accounts_.emplace(std::string{acct.name().display()}, std::move(acct));
    }

    core::Result<domain::identity::Account, core::Error>
    find_by_name(std::string_view name) override {
        auto it = accounts_.find(std::string{name});
        if (it == accounts_.end()) {
            return core::fail(core::Error{core::StatusCode::NotFound, "not found"});
        }
        return it->second;
    }

    core::Result<domain::identity::Account, core::Error>
    find_by_id(uint32_t id) override {
        for (auto& [_, a] : accounts_) {
            if (a.id().value() == id) return a;
        }
        return core::fail(core::Error{core::StatusCode::NotFound, "not found"});
    }

    core::Result<void, core::Error> save(const domain::identity::Account&) override { return {}; }
    core::Result<void, core::Error> remove(std::string_view) override { return {}; }
    core::Result<bool, core::Error> exists(std::string_view) override { return false; }
    core::Result<std::vector<domain::identity::Account>, core::Error> list_online() override { return std::vector<domain::identity::Account>{}; }
    core::Result<uint32_t, core::Error> count() override { return 0u; }

private:
    std::unordered_map<std::string, domain::identity::Account> accounts_;
};

}  // namespace

// ---------------------------------------------------------------------------
// Test cases
// ---------------------------------------------------------------------------

TEST_CASE("GetLadderPage: happy path — page 1 of 3 entries",
          "[application][ladder][get_page]") {
    FakeLadderRepository ladder_repo;
    FakeAccountRepository account_repo;

    account_repo.add(make_account(domain::AccountId{1}, "Alice"));
    account_repo.add(make_account(domain::AccountId{2}, "Bobbie"));
    account_repo.add(make_account(domain::AccountId{3}, "Charlie"));

    ladder_repo.entries = {
        {domain::AccountId{1}, 1600, 10, 1, 0},
        {domain::AccountId{2}, 1500, 7,  2, 0},
        {domain::AccountId{3}, 1400, 3,  5, 1},
    };

    GetLadderPage uc{ladder_repo, account_repo};
    auto result = uc.execute(GetLadderPageQuery{"STAR", 1, 10});

    REQUIRE(result);
    const auto& page = result.value();
    REQUIRE(page.page == 1u);
    REQUIRE(page.page_size == 10u);
    REQUIRE(page.total_entries == 3u);
    REQUIRE(page.entries.size() == 3u);
    REQUIRE(page.entries[0].account_id == domain::AccountId{1});
    REQUIRE(page.entries[0].rank == 1u);
    REQUIRE(page.entries[1].account_id == domain::AccountId{2});
    REQUIRE(page.entries[1].rank == 2u);
    REQUIRE(page.entries[2].account_id == domain::AccountId{3});
    REQUIRE(page.entries[2].rank == 3u);
}

TEST_CASE("GetLadderPage: page_size > 100 returns InvalidArgument",
          "[application][ladder][get_page]") {
    FakeLadderRepository ladder_repo;
    FakeAccountRepository account_repo;

    GetLadderPage uc{ladder_repo, account_repo};
    auto result = uc.execute(GetLadderPageQuery{"STAR", 1, 101});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("GetLadderPage: page beyond total returns empty entries list",
          "[application][ladder][get_page]") {
    FakeLadderRepository ladder_repo;
    FakeAccountRepository account_repo;

    ladder_repo.entries = {
        {domain::AccountId{1}, 1600, 10, 1, 0},
        {domain::AccountId{2}, 1500, 7,  2, 0},
    };

    GetLadderPage uc{ladder_repo, account_repo};
    // page 5 with page_size 10 — far beyond 2 total entries
    auto result = uc.execute(GetLadderPageQuery{"STAR", 5, 10});

    REQUIRE(result);
    REQUIRE(result.value().total_entries == 2u);
    REQUIRE(result.value().entries.empty());
}
