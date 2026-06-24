// SPDX-License-Identifier: GPL-2.0-or-later
//
// Behavioural tests for Account aggregate methods the existing
// account_test.cpp does not reach: the must-change-password rotation flags
// (require_password_change / clear_password_change_requirement and their
// edge-only event emission + idempotency), change_password's interaction with
// the rotation flag, unlock(), apply_ban/clear_ban, revoke_command_group,
// and the password_hash1()/locale() accessors.

#include <chrono>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "domain/identity/account.hpp"

using namespace pvpgn;
using domain::identity::Account;
using domain::AccountId;
using domain::Ban;
using domain::BanScope;
using domain::BNHash;
using domain::Locale;
using domain::UserName;

namespace {

Account make_account(const char* name = "Alice", char hash_byte = 0x42) {
    auto u = UserName::parse(name).value();
    auto h = BNHash::from_bytes(std::string(20, hash_byte)).value();
    return Account::create(AccountId{1}, std::move(u), std::move(h),
                           Locale::parse_or_default("enUS")).value();
}

}  // namespace

TEST_CASE("Account::require_password_change flips the flag once and emits on the edge",
          "[domain][identity][rotation]") {
    auto a = make_account();
    (void)a.drain_events();
    CHECK_FALSE(a.must_change_password());

    a.require_password_change();
    CHECK(a.must_change_password());

    auto evs = a.drain_events();
    REQUIRE(evs.size() == 1);
    CHECK(std::holds_alternative<domain::events::AccountPasswordRotationRequired>(evs[0]));

    // Idempotent: a second call on the already-set flag emits nothing.
    a.require_password_change();
    CHECK(a.must_change_password());
    CHECK(a.drain_events().empty());
}

TEST_CASE("Account::clear_password_change_requirement clears + emits only on the edge",
          "[domain][identity][rotation]") {
    auto a = make_account();
    (void)a.drain_events();

    // No-op when already clear: nothing emitted, flag stays false.
    a.clear_password_change_requirement();
    CHECK_FALSE(a.must_change_password());
    CHECK(a.drain_events().empty());

    a.require_password_change();
    (void)a.drain_events();

    a.clear_password_change_requirement();
    CHECK_FALSE(a.must_change_password());
    auto evs = a.drain_events();
    REQUIRE(evs.size() == 1);
    CHECK(std::holds_alternative<domain::events::AccountPasswordRotationCleared>(evs[0]));
}

TEST_CASE("Account::change_password while rotation-required clears flag + emits both events",
          "[domain][identity][rotation]") {
    auto a = make_account("Bob", 0x11);
    a.require_password_change();
    (void)a.drain_events();
    CHECK(a.must_change_password());

    auto new_hash = BNHash::from_bytes(std::string(20, 0x22)).value();
    a.change_password(new_hash);
    CHECK_FALSE(a.must_change_password());

    auto evs = a.drain_events();
    REQUIRE(evs.size() == 2);
    CHECK(std::holds_alternative<domain::events::AccountPasswordChanged>(evs[0]));
    CHECK(std::holds_alternative<domain::events::AccountPasswordRotationCleared>(evs[1]));
}

TEST_CASE("Account::change_password without rotation-required emits only the change event",
          "[domain][identity][rotation]") {
    auto a = make_account("Bob", 0x11);
    (void)a.drain_events();
    CHECK_FALSE(a.must_change_password());

    auto new_hash = BNHash::from_bytes(std::string(20, 0x33)).value();
    a.change_password(new_hash);

    auto evs = a.drain_events();
    REQUIRE(evs.size() == 1);
    CHECK(std::holds_alternative<domain::events::AccountPasswordChanged>(evs[0]));
}

TEST_CASE("Account::unlock undoes lock and re-permits login",
          "[domain][identity]") {
    auto a = make_account("Bob", 0x33);
    a.lock();
    CHECK(a.is_locked());
    a.unlock();
    CHECK_FALSE(a.is_locked());
    (void)a.drain_events();

    auto good = BNHash::from_bytes(std::string(20, 0x33)).value();
    CHECK(a.login(good, domain::IpAddress::parse("10.0.0.1").value(),
                  domain::ClientTag::parse("STAR").value(),
                  std::chrono::system_clock::time_point{}) ==
          Account::LoginOutcome::Accepted);
}

TEST_CASE("Account::apply_ban records the ban and emits AccountBanned",
          "[domain][identity][ban]") {
    auto a = make_account();
    (void)a.drain_events();
    const auto now = std::chrono::system_clock::time_point{};
    a.apply_ban(Ban{BanScope::Account, "abuse", AccountId{99}, now,
                    now + std::chrono::hours(1)});

    REQUIRE(a.ban().has_value());
    CHECK(a.ban()->reason == "abuse");
    auto evs = a.drain_events();
    REQUIRE(evs.size() == 1);
    CHECK(std::holds_alternative<domain::events::AccountBanned>(evs[0]));
}

TEST_CASE("Account::clear_ban: emits on present ban, no-op when none",
          "[domain][identity][ban]") {
    auto a = make_account();
    (void)a.drain_events();

    // No ban yet: clear is a silent no-op.
    a.clear_ban();
    CHECK_FALSE(a.ban().has_value());
    CHECK(a.drain_events().empty());

    const auto now = std::chrono::system_clock::time_point{};
    a.apply_ban(Ban{BanScope::Account, "abuse", AccountId{99}, now,
                    now + std::chrono::hours(1)});
    (void)a.drain_events();

    a.clear_ban();
    CHECK_FALSE(a.ban().has_value());
    auto evs = a.drain_events();
    REQUIRE(evs.size() == 1);
    CHECK(std::holds_alternative<domain::events::AccountUnbanned>(evs[0]));
}

TEST_CASE("Account::revoke_command_group clears a granted bit silently",
          "[domain][identity][permissions]") {
    auto a = make_account();
    a.grant_command_group(5);
    (void)a.drain_events();
    CHECK(a.command_groups().has(5));

    // revoke_command_group emits no event (unlike grant).
    a.revoke_command_group(5);
    CHECK_FALSE(a.command_groups().has(5));
    CHECK(a.drain_events().empty());
}

TEST_CASE("Account::password_hash1 exposes the stored hash for transcript re-derivation",
          "[domain][identity]") {
    auto a = make_account("Eve", 0x5A);
    auto expected = BNHash::from_bytes(std::string(20, 0x5A)).value();
    CHECK(a.password_hash1() == expected);

    auto rotated = BNHash::from_bytes(std::string(20, 0x6B)).value();
    a.change_password(rotated);
    CHECK(a.password_hash1() == rotated);
}

TEST_CASE("Account::locale accessor returns the constructed locale",
          "[domain][identity]") {
    auto u = UserName::parse("Frank").value();
    auto h = BNHash::from_bytes(std::string(20, 0x42)).value();
    auto a = Account::create(AccountId{3}, std::move(u), std::move(h),
                             Locale::parse_or_default("deDE")).value();
    CHECK(std::string{a.locale().text()} == "deDE");
}

TEST_CASE("CommandGroupMask::any reflects whether any group is granted",
          "[domain][identity][permissions]") {
    using domain::identity::CommandGroupMask;
    CommandGroupMask mask;
    CHECK_FALSE(mask.any());
    mask.grant(4);
    CHECK(mask.any());
    mask.revoke(4);
    CHECK_FALSE(mask.any());
}
