// SPDX-License-Identifier: GPL-2.0-or-later
//
// Additional pagination-boundary tests for `application::ladder::GetLadderPage`.
// Uses inline fakes for ILadderRepository and IAccountRepository, mirroring
// get_ladder_page_test.cpp.

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "application/ladder/get_ladder_page.hpp"
#include "domain/identity/ports.hpp"
#include "domain/ladder/ports.hpp"
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

class FakeLadderRepository final : public domain::ladder::ILadderRepository {
public:
    std::vector<domain::ladder::LadderEntry> entries;

    core::Result<uint32_t, core::Error>
    get_rank(domain::AccountId /*account_id*/) override {
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

class FakeAccountRepository final : public domain::identity::IAccountRepository {
public:
    void add(domain::identity::Account acct) {
        accounts_.emplace(std::string{acct.name().display()}, std::move(acct));
    }

    core::Result<domain::identity::Account>
    find_by_name(const domain::UserName& name) const override {
        auto it = accounts_.find(std::string{name.display()});
        if (it == accounts_.end()) {
            return core::fail(core::Error{core::StatusCode::NotFound, "not found"});
        }
        return it->second;
    }

    core::Result<domain::identity::Account>
    find_by_id(domain::AccountId id) const override {
        for (const auto& [_, a] : accounts_) {
            if (a.id().value() == id.value()) return a;
        }
        return core::fail(core::Error{core::StatusCode::NotFound, "not found"});
    }

    core::Status<> save(const domain::identity::Account&) override { return core::ok(); }
    core::Status<> remove(domain::AccountId) override { return core::ok(); }
    void forEach(std::function<bool(const domain::identity::Account&)>) const override {}
    std::size_t size() const noexcept override { return accounts_.size(); }

private:
    std::unordered_map<std::string, domain::identity::Account> accounts_;
};

// Build a ladder repo with `count` synthetic entries (account ids 1..count),
// descending rating so the order is stable. Also registers accounts.
void seed(FakeLadderRepository& ladder, FakeAccountRepository& accounts,
          std::uint32_t count) {
    for (std::uint32_t i = 1; i <= count; ++i) {
        accounts.add(make_account(domain::AccountId{i},
                                  "Player" + std::to_string(i)));
        domain::ladder::LadderEntry e;
        e.account     = domain::AccountId{i};
        e.rating      = static_cast<std::int32_t>(2000 - i);
        e.wins        = i;
        e.losses      = 0;
        e.disconnects = 0;
        ladder.entries.push_back(e);
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Test cases — pagination boundaries
// ---------------------------------------------------------------------------

TEST_CASE("GetLadderPage: empty ladder yields zero entries and zero total",
          "[application][ladder][get_page]") {
    FakeLadderRepository ladder_repo;
    FakeAccountRepository account_repo;

    GetLadderPage uc{ladder_repo, account_repo};
    auto result = uc.execute(GetLadderPageQuery{"STAR", 1, 10});

    REQUIRE(result);
    const auto& page = result.value();
    REQUIRE(page.total_entries == 0u);
    REQUIRE(page.entries.empty());
    REQUIRE(page.page == 1u);
    REQUIRE(page.page_size == 10u);
}

TEST_CASE("GetLadderPage: first page of a multi-page ladder",
          "[application][ladder][get_page]") {
    FakeLadderRepository ladder_repo;
    FakeAccountRepository account_repo;
    seed(ladder_repo, account_repo, 25);

    GetLadderPage uc{ladder_repo, account_repo};
    auto result = uc.execute(GetLadderPageQuery{"STAR", 1, 10});

    REQUIRE(result);
    const auto& page = result.value();
    REQUIRE(page.total_entries == 25u);
    REQUIRE(page.entries.size() == 10u);
    REQUIRE(page.entries.front().rank == 1u);
    REQUIRE(page.entries.back().rank == 10u);
    REQUIRE(page.entries.front().account_id == domain::AccountId{1});
    // Account name lookup populated.
    REQUIRE(page.entries.front().account_name == "Player1");
}

TEST_CASE("GetLadderPage: last partial page returns the remainder",
          "[application][ladder][get_page]") {
    FakeLadderRepository ladder_repo;
    FakeAccountRepository account_repo;
    seed(ladder_repo, account_repo, 25);

    // 25 entries / 10 per page => page 3 holds the final 5 entries.
    GetLadderPage uc{ladder_repo, account_repo};
    auto result = uc.execute(GetLadderPageQuery{"STAR", 3, 10});

    REQUIRE(result);
    const auto& page = result.value();
    REQUIRE(page.total_entries == 25u);
    REQUIRE(page.entries.size() == 5u);
    REQUIRE(page.entries.front().rank == 21u);
    REQUIRE(page.entries.back().rank == 25u);
    REQUIRE(page.entries.back().account_id == domain::AccountId{25});
}

TEST_CASE("GetLadderPage: page exactly one past the end is empty",
          "[application][ladder][get_page]") {
    FakeLadderRepository ladder_repo;
    FakeAccountRepository account_repo;
    seed(ladder_repo, account_repo, 20);

    // offset == total => out-of-range, empty slice but valid total.
    GetLadderPage uc{ladder_repo, account_repo};
    auto result = uc.execute(GetLadderPageQuery{"STAR", 3, 10});

    REQUIRE(result);
    const auto& page = result.value();
    REQUIRE(page.total_entries == 20u);
    REQUIRE(page.entries.empty());
    REQUIRE(page.page == 3u);
}

TEST_CASE("GetLadderPage: page_size larger than total returns all on page 1",
          "[application][ladder][get_page]") {
    FakeLadderRepository ladder_repo;
    FakeAccountRepository account_repo;
    seed(ladder_repo, account_repo, 3);

    GetLadderPage uc{ladder_repo, account_repo};
    auto result = uc.execute(GetLadderPageQuery{"STAR", 1, 100});

    REQUIRE(result);
    const auto& page = result.value();
    REQUIRE(page.total_entries == 3u);
    REQUIRE(page.entries.size() == 3u);
    REQUIRE(page.entries.front().rank == 1u);
    REQUIRE(page.entries.back().rank == 3u);
}

TEST_CASE("GetLadderPage: page < 1 returns InvalidArgument",
          "[application][ladder][get_page]") {
    FakeLadderRepository ladder_repo;
    FakeAccountRepository account_repo;

    GetLadderPage uc{ladder_repo, account_repo};
    auto result = uc.execute(GetLadderPageQuery{"STAR", 0, 10});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("GetLadderPage: page_size < 1 returns InvalidArgument",
          "[application][ladder][get_page]") {
    FakeLadderRepository ladder_repo;
    FakeAccountRepository account_repo;

    GetLadderPage uc{ladder_repo, account_repo};
    auto result = uc.execute(GetLadderPageQuery{"STAR", 1, 0});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("GetLadderPage: empty ladder_id returns InvalidArgument",
          "[application][ladder][get_page]") {
    FakeLadderRepository ladder_repo;
    FakeAccountRepository account_repo;

    GetLadderPage uc{ladder_repo, account_repo};
    auto result = uc.execute(GetLadderPageQuery{"", 1, 10});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("GetLadderPage: entry for missing account leaves name empty",
          "[application][ladder][get_page]") {
    FakeLadderRepository ladder_repo;
    FakeAccountRepository account_repo;
    // Ladder entry exists but the account is NOT registered in the account repo.
    domain::ladder::LadderEntry e;
    e.account = domain::AccountId{99};
    e.rating  = 1700;
    e.wins    = 4;
    ladder_repo.entries.push_back(e);

    GetLadderPage uc{ladder_repo, account_repo};
    auto result = uc.execute(GetLadderPageQuery{"STAR", 1, 10});

    REQUIRE(result);
    const auto& page = result.value();
    REQUIRE(page.entries.size() == 1u);
    REQUIRE(page.entries.front().account_id == domain::AccountId{99});
    REQUIRE(page.entries.front().account_name.empty());
    REQUIRE(page.entries.front().rank == 1u);
}
