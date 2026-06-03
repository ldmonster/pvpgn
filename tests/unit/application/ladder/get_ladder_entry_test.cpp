// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::ladder::GetLadderEntry`.
// Uses inline fakes for ILadderRepository and IAccountRepository.

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
// ---------------------------------------------------------------------------

class FakeLadderRepository final : public domain::ladder::ILadderRepository {
public:
    std::vector<domain::ladder::LadderEntry> entries;

    core::Result<uint32_t, core::Error>
    get_rank(domain::AccountId /*account_id*/) override {
        // Return rank 1 if any entries exist (simplified for tests)
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

    // Current IAccountRepository port (domain::AccountId / UserName, const).
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

TEST_CASE("GetLadderEntry: happy path — entry found with correct data",
          "[application][ladder][get_entry]") {
    FakeLadderRepository ladder_repo;
    FakeAccountRepository account_repo;

    const domain::AccountId alice_id{1};
    account_repo.add(make_account(alice_id, "Alice"));
    ladder_repo.entries = {{alice_id, 1600, 10, 2, 1}};

    GetLadderEntry uc{ladder_repo, account_repo};
    auto result = uc.execute(GetLadderEntryQuery{"STAR", alice_id});

    REQUIRE(result);
    REQUIRE(result.value().account_id == alice_id);
    REQUIRE(result.value().wins == 10);
    REQUIRE(result.value().losses == 2);
    REQUIRE(result.value().disconnects == 1);
    REQUIRE(result.value().rating == 1600);
    REQUIRE(result.value().rank == 1u);
}

TEST_CASE("GetLadderEntry: account not in ladder returns NotFound",
          "[application][ladder][get_entry]") {
    FakeLadderRepository ladder_repo;
    FakeAccountRepository account_repo;

    const domain::AccountId bob_id{2};
    account_repo.add(make_account(bob_id, "Bobbie"));
    // ladder_repo has no entries for bob

    GetLadderEntry uc{ladder_repo, account_repo};
    auto result = uc.execute(GetLadderEntryQuery{"STAR", bob_id});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code() == core::StatusCode::NotFound);
}
