// SPDX-License-Identifier: GPL-2.0-or-later
//
// Edge/error-branch tests for the ladder use-cases that the
// happy-path suites (get_ladder_entry_test.cpp, get_ladder_entry_lookup_test.cpp,
// recompute_ladder_test.cpp) do not reach:
//
//   GetLadderEntry:
//     * get_top_n() returns an error -> the error is propagated.
//     * account is ranked (get_rank succeeds) but absent from get_top_n
//       (repository inconsistency) -> falls through to NotFound.
//
//   RecomputeLadder:
//     * save_entry() fails mid-iteration -> error is propagated.
//     * tie on wins resolved by rating descending (the secondary
//       comparator branch of the stable_sort lambda).

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "application/ladder/get_ladder_entry.hpp"
#include "application/ladder/recompute_ladder.hpp"
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
using application::ladder::RecomputeLadder;
using application::ladder::RecomputeLadderCommand;

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

// ---------------------------------------------------------------------------
// Inline fake: ILadderRepository with configurable failure injection.
// ---------------------------------------------------------------------------

class ConfigurableLadderRepository final
    : public domain::ladder::ILadderRepository {
public:
    std::vector<domain::ladder::LadderEntry> entries;

    // When set, get_rank reports this rank unconditionally (used to force the
    // "account is ranked but missing from get_top_n" inconsistency branch).
    bool     force_rank        = false;
    uint32_t forced_rank_value = 1u;

    // When set, get_top_n returns an error instead of the stored entries.
    bool top_n_fails = false;

    // When >= 0, save_entry fails on the Nth (zero-based) call.
    int  fail_save_on_call = -1;
    int  save_calls        = 0;

    core::Result<uint32_t, core::Error>
    get_rank(domain::AccountId account_id) override {
        if (force_rank) return forced_rank_value;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].account == account_id) {
                return static_cast<uint32_t>(i + 1);
            }
        }
        return core::fail(core::Error{core::StatusCode::NotFound, "no rank"});
    }

    core::Result<void, core::Error>
    save_entry(const domain::ladder::LadderEntry& entry) override {
        const int call = save_calls++;
        if (fail_save_on_call >= 0 && call == fail_save_on_call) {
            return core::fail(core::Error{core::StatusCode::Internal,
                                          "save failed"});
        }
        std::erase_if(entries, [&](const domain::ladder::LadderEntry& e) {
            return e.account == entry.account;
        });
        entries.push_back(entry);
        return core::ok();
    }

    core::Result<std::vector<domain::ladder::LadderEntry>, core::Error>
    get_top_n(uint32_t n) override {
        if (top_n_fails) {
            return core::fail(core::Error{core::StatusCode::Internal,
                                          "top_n failed"});
        }
        auto copy = entries;
        if (copy.size() > n) copy.resize(n);
        return copy;
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// GetLadderEntry edge cases
// ---------------------------------------------------------------------------

TEST_CASE("GetLadderEntry: get_top_n failure is propagated",
          "[application][ladder][get_entry]") {
    ConfigurableLadderRepository ladder_repo;
    FakeAccountRepository account_repo;

    const domain::AccountId alice{1};
    account_repo.add(make_account(alice, "Alice"));
    ladder_repo.entries = {{alice, 1600, 10, 2, 1}};  // so get_rank succeeds
    ladder_repo.top_n_fails = true;

    GetLadderEntry uc{ladder_repo, account_repo};
    auto result = uc.execute(GetLadderEntryQuery{"STAR", alice});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code() == core::StatusCode::Internal);
}

TEST_CASE("GetLadderEntry: ranked account absent from top_n returns NotFound",
          "[application][ladder][get_entry]") {
    ConfigurableLadderRepository ladder_repo;
    FakeAccountRepository account_repo;

    const domain::AccountId ghost{7};
    account_repo.add(make_account(ghost, "Ghost"));
    // get_rank reports a rank for ghost, but get_top_n contains no entry for
    // it -> the lookup loop falls through to the trailing NotFound.
    ladder_repo.force_rank = true;
    ladder_repo.entries    = {{domain::AccountId{1}, 1500, 5, 5, 0}};

    GetLadderEntry uc{ladder_repo, account_repo};
    auto result = uc.execute(GetLadderEntryQuery{"STAR", ghost});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code() == core::StatusCode::NotFound);
}

// ---------------------------------------------------------------------------
// RecomputeLadder edge cases
// ---------------------------------------------------------------------------

TEST_CASE("RecomputeLadder: save_entry failure is propagated",
          "[application][ladder][recompute]") {
    ConfigurableLadderRepository repo;
    repo.entries = {
        {domain::AccountId{1}, 1500, 5, 2, 0},
        {domain::AccountId{2}, 1600, 10, 1, 0},
    };
    repo.fail_save_on_call = 1;  // fail on the second saved entry

    RecomputeLadder uc{repo};
    auto result = uc.execute(RecomputeLadderCommand{"STAR"});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code() == core::StatusCode::Internal);
}

TEST_CASE("RecomputeLadder: equal wins broken by rating descending",
          "[application][ladder][recompute]") {
    ConfigurableLadderRepository repo;
    // Same win count (7) -> tie-break exercises the secondary rating
    // comparator. Higher rating must rank first.
    repo.entries = {
        {domain::AccountId{1}, 1500, 7, 3, 0},  // lower rating
        {domain::AccountId{2}, 1800, 7, 3, 0},  // higher rating -> rank 1
        {domain::AccountId{3}, 1650, 7, 3, 0},  // middle rating  -> rank 2
    };

    RecomputeLadder uc{repo};
    auto result = uc.execute(RecomputeLadderCommand{"STAR"});

    REQUIRE(result);
    REQUIRE(result.value().entries_updated == 3);
    REQUIRE(repo.entries[0].account == domain::AccountId{2});  // 1800
    REQUIRE(repo.entries[1].account == domain::AccountId{3});  // 1650
    REQUIRE(repo.entries[2].account == domain::AccountId{1});  // 1500
}
