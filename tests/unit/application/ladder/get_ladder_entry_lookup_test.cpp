// SPDX-License-Identifier: GPL-2.0-or-later
//
// Additional lookup/validation tests for `application::ladder::GetLadderEntry`.
// Uses inline fakes for ILadderRepository and IAccountRepository, mirroring
// get_ladder_entry_test.cpp.

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "application/ladder/get_ladder_entry.hpp"
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
using application::ladder::GetLadderEntry;
using application::ladder::GetLadderEntryQuery;
using application::ladder::LadderEntryResult;

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
//
// get_rank() reports a rank only when the queried account actually has an
// entry, so that the "unranked account" branch in GetLadderEntry is exercised.
// ---------------------------------------------------------------------------

class FakeLadderRepository final : public domain::ladder::ILadderRepository {
public:
    std::vector<domain::ladder::LadderEntry> entries;

    core::Result<uint32_t, core::Error>
    get_rank(domain::AccountId account_id) override {
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].account == account_id) {
                return static_cast<uint32_t>(i + 1);
            }
        }
        return core::fail(core::Error{core::StatusCode::NotFound, "no rank"});
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

}  // namespace

// ---------------------------------------------------------------------------
// Test cases
// ---------------------------------------------------------------------------

TEST_CASE("GetLadderEntry: returns correct rank for second-place account",
          "[application][ladder][get_entry]") {
    FakeLadderRepository ladder_repo;
    FakeAccountRepository account_repo;

    const domain::AccountId alice{1};
    const domain::AccountId bob{2};
    account_repo.add(make_account(alice, "Alice"));
    account_repo.add(make_account(bob, "Bobbie"));
    ladder_repo.entries = {
        {alice, 1700, 12, 1, 0},
        {bob,   1600,  8, 3, 2},
    };

    GetLadderEntry uc{ladder_repo, account_repo};
    auto result = uc.execute(GetLadderEntryQuery{"STAR", bob});

    REQUIRE(result);
    const auto& r = result.value();
    REQUIRE(r.account_id == bob);
    REQUIRE(r.account_name == "Bobbie");
    REQUIRE(r.rank == 2u);
    REQUIRE(r.wins == 8);
    REQUIRE(r.losses == 3);
    REQUIRE(r.disconnects == 2);
    REQUIRE(r.rating == 1600);
}

TEST_CASE("GetLadderEntry: empty ladder_id returns InvalidArgument",
          "[application][ladder][get_entry]") {
    FakeLadderRepository ladder_repo;
    FakeAccountRepository account_repo;

    const domain::AccountId alice{1};
    account_repo.add(make_account(alice, "Alice"));
    ladder_repo.entries = {{alice, 1600, 10, 2, 1}};

    GetLadderEntry uc{ladder_repo, account_repo};
    auto result = uc.execute(GetLadderEntryQuery{"", alice});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("GetLadderEntry: unknown account returns NotFound",
          "[application][ladder][get_entry]") {
    FakeLadderRepository ladder_repo;
    FakeAccountRepository account_repo;
    // No accounts registered at all.

    GetLadderEntry uc{ladder_repo, account_repo};
    auto result = uc.execute(GetLadderEntryQuery{"STAR", domain::AccountId{42}});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("GetLadderEntry: known account with no ladder entry returns NotFound",
          "[application][ladder][get_entry]") {
    FakeLadderRepository ladder_repo;
    FakeAccountRepository account_repo;

    const domain::AccountId alice{1};
    const domain::AccountId bob{2};
    account_repo.add(make_account(alice, "Alice"));
    account_repo.add(make_account(bob, "Bobbie"));
    // Only alice is ranked; bob exists but is unranked.
    ladder_repo.entries = {{alice, 1600, 10, 2, 1}};

    GetLadderEntry uc{ladder_repo, account_repo};
    auto result = uc.execute(GetLadderEntryQuery{"STAR", bob});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code() == core::StatusCode::NotFound);
}
