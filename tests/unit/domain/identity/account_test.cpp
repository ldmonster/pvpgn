// SPDX-License-Identifier: GPL-2.0-or-later
#include <chrono>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "domain/identity/account.hpp"

using namespace pvpgn;
using domain::identity::Account;
using domain::AccountId;
using domain::BNHash;
using domain::ClientTag;
using domain::IpAddress;
using domain::Locale;
using domain::UserName;

namespace {

Account make_account(const char* name = "Alice", char hash_byte = 0x42) {
    auto u = UserName::parse(name).value();
    auto h = BNHash::from_bytes(std::string(20, hash_byte)).value();
    return Account::create(AccountId{1}, std::move(u), std::move(h),
                           Locale::parse_or_default("enUS")).value();
}

const ClientTag kStar = ClientTag::parse("STAR").value();
const IpAddress kLocal = IpAddress::parse("10.0.0.1").value();

}  // namespace

TEST_CASE("Account::create emits AccountCreated and exposes identity",
          "[domain][identity]") {
    auto a = make_account("Alice", 0x11);
    auto evs = a.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::AccountCreated>(evs[0]));
    REQUIRE(a.id() == AccountId{1});
    REQUIRE(std::string{a.name().display()} == "Alice");
    REQUIRE_FALSE(a.is_admin());
    REQUIRE_FALSE(a.is_locked());
}

TEST_CASE("Account::login: correct hash -> Accepted + UserLoggedIn",
          "[domain][identity][login]") {
    auto a = make_account("Bob", 0x33);
    (void)a.drain_events();
    auto good = BNHash::from_bytes(std::string(20, 0x33)).value();
    auto outcome = a.login(good, kLocal, kStar,
                           std::chrono::system_clock::time_point{});
    REQUIRE(outcome == Account::LoginOutcome::Accepted);
    auto evs = a.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::UserLoggedIn>(evs[0]));
    const auto& ev = std::get<domain::events::UserLoggedIn>(evs[0]);
    REQUIRE(ev.id == a.id());
    REQUIRE(ev.ip == kLocal);
    REQUIRE(ev.tag == kStar);
}

TEST_CASE("Account::login: wrong hash -> InvalidCredentials + rejection event",
          "[domain][identity][login]") {
    auto a = make_account("Bob", 0x33);
    (void)a.drain_events();
    auto bad = BNHash::from_bytes(std::string(20, 0x77)).value();
    auto outcome = a.login(bad, kLocal, kStar,
                           std::chrono::system_clock::time_point{});
    REQUIRE(outcome == Account::LoginOutcome::InvalidCredentials);
    auto evs = a.drain_events();
    REQUIRE(evs.size() == 1);
    const auto& ev = std::get<domain::events::UserLoginRejected>(evs[0]);
    REQUIRE(ev.reason == domain::events::UserLoginRejected::Reason::InvalidCredentials);
}

TEST_CASE("Account::login: active ban blocks even with correct hash",
          "[domain][identity][login][ban]") {
    auto a = make_account("Bob", 0x33);
    (void)a.drain_events();

    const auto now = std::chrono::system_clock::time_point{};
    const auto in_an_hour = now + std::chrono::hours(1);
    a.apply_ban(domain::Ban{
        domain::BanScope::Account, "spam", AccountId{99}, now, in_an_hour});
    (void)a.drain_events();

    auto good = BNHash::from_bytes(std::string(20, 0x33)).value();
    auto outcome = a.login(good, kLocal, kStar, now);
    REQUIRE(outcome == Account::LoginOutcome::Banned);

    auto evs = a.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::get<domain::events::UserLoginRejected>(evs[0]).reason ==
            domain::events::UserLoginRejected::Reason::AccountBanned);
}

TEST_CASE("Account::login: expired ban is cleared and login proceeds",
          "[domain][identity][login][ban]") {
    auto a = make_account("Bob", 0x33);
    (void)a.drain_events();

    const auto issued = std::chrono::system_clock::time_point{};
    const auto expired_at = issued + std::chrono::minutes(5);
    a.apply_ban(domain::Ban{
        domain::BanScope::Account, "spam", AccountId{99}, issued, expired_at});
    (void)a.drain_events();

    const auto later = expired_at + std::chrono::minutes(1);
    auto good = BNHash::from_bytes(std::string(20, 0x33)).value();
    auto outcome = a.login(good, kLocal, kStar, later);
    REQUIRE(outcome == Account::LoginOutcome::Accepted);
    REQUIRE_FALSE(a.ban().has_value());

    auto evs = a.drain_events();
    REQUIRE(evs.size() == 2);
    REQUIRE(std::holds_alternative<domain::events::AccountUnbanned>(evs[0]));
    REQUIRE(std::holds_alternative<domain::events::UserLoggedIn>(evs[1]));
}

TEST_CASE("Account: lock blocks login independently of credentials",
          "[domain][identity][login]") {
    auto a = make_account("Bob", 0x33);
    a.lock();
    (void)a.drain_events();
    auto good = BNHash::from_bytes(std::string(20, 0x33)).value();
    REQUIRE(a.login(good, kLocal, kStar,
                    std::chrono::system_clock::time_point{}) ==
            Account::LoginOutcome::Locked);
}

TEST_CASE("Account: grant_command_group is idempotent + admin detection",
          "[domain][identity][groups]") {
    auto a = make_account();
    (void)a.drain_events();

    a.grant_command_group(1);
    a.grant_command_group(1);  // idempotent — no second event
    a.grant_command_group(7);  // admin tier

    auto evs = a.drain_events();
    REQUIRE(evs.size() == 2);
    REQUIRE(std::holds_alternative<domain::events::AccountCommandGroupGranted>(evs[0]));
    REQUIRE(std::holds_alternative<domain::events::AccountCommandGroupGranted>(evs[1]));
    REQUIRE(a.command_groups().has(1));
    REQUIRE(a.command_groups().has(7));
    REQUIRE(a.is_admin());
}

TEST_CASE("Account::change_password updates hash + emits event",
          "[domain][identity]") {
    auto a = make_account("Bob", 0x11);
    (void)a.drain_events();
    auto new_hash = BNHash::from_bytes(std::string(20, 0x22)).value();
    a.change_password(new_hash);

    auto evs = a.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::AccountPasswordChanged>(evs[0]));

    auto outcome = a.login(new_hash, kLocal, kStar,
                           std::chrono::system_clock::time_point{});
    REQUIRE(outcome == Account::LoginOutcome::Accepted);
}

TEST_CASE("CommandGroupMask: boundary validation of group numbers",
          "[domain][identity][permissions]") {
    using domain::identity::CommandGroupMask;
    // Valid groups are exactly 1..kBits (8). These cases pin every boundary so
    // the `group >= 1 && group <= kBits` guards in grant/revoke/has are exact
    // (mutation pilot: catches >=->>, <=-><, &&->|| on those lines).
    CommandGroupMask mask;

    // Below the low bound: 0 must be rejected by grant and has.
    mask.grant(0);
    CHECK_FALSE(mask.has(0));
    CHECK_FALSE(mask.any());

    // The low bound itself: 1 is valid.
    mask.grant(1);
    CHECK(mask.has(1));

    // The high bound itself: kBits (8) is valid (kills <=-><, which would
    // reject 8).
    mask.grant(8);
    CHECK(mask.has(8));

    // Just past the high bound: 9 must be rejected (kills >=->> only-direction
    // and any off-by-one that would admit 9).
    mask.grant(9);
    CHECK_FALSE(mask.has(9));

    // has() with an out-of-range group returns false even when bits are set —
    // exercises the && short-circuit (kills &&->||, which would dereference an
    // out-of-range bit).
    CHECK_FALSE(mask.has(0));
    CHECK_FALSE(mask.has(9));

    // revoke() respects the same bounds. Out-of-range revokes are no-ops (and
    // must not touch an out-of-range bit — kills &&->|| on the revoke line).
    mask.revoke(0);
    mask.revoke(9);
    CHECK(mask.has(1));
    CHECK(mask.has(8));

    // revoke at the low bound (1) undoes the grant — kills >=->> on revoke.
    mask.revoke(1);
    CHECK_FALSE(mask.has(1));

    // revoke at the high bound (8 == kBits) undoes the grant — kills <=-><
    // on revoke, which would refuse to clear group 8.
    mask.revoke(8);
    CHECK_FALSE(mask.has(8));
    CHECK_FALSE(mask.any());
}

TEST_CASE("CommandGroupMask: admin tiers and value equality",
          "[domain][identity][permissions]") {
    using domain::identity::CommandGroupMask;
    // is_admin() is groups 7 OR 8 (legacy admin). Each alone must qualify --
    // kills the ||->&& mutant on `test(6) || test(7)`.
    CommandGroupMask only7;
    only7.grant(7);
    CHECK(only7.is_admin());
    CommandGroupMask only8;
    only8.grant(8);
    CHECK(only8.is_admin());
    CommandGroupMask only1;
    only1.grant(1);
    CHECK_FALSE(only1.is_admin());

    // Value equality must be exact -- exercises the defaulted operator== so a
    // mutation of it is caught (by behaviour, or by failing to compile).
    CommandGroupMask a;
    CommandGroupMask b;
    a.grant(3);
    b.grant(3);
    CHECK(a == b);
    b.grant(4);
    CHECK(a != b);
    CHECK_FALSE(a == b);
}

TEST_CASE("Account::verify_password matches only the stored hash",
          "[domain][identity]") {
    // Kills the ==->!= mutant on `password_ == candidate`.
    auto a = make_account("Eve", 0x33);
    auto correct = BNHash::from_bytes(std::string(20, 0x33)).value();
    auto wrong   = BNHash::from_bytes(std::string(20, 0x44)).value();
    CHECK(a.verify_password(correct));
    CHECK_FALSE(a.verify_password(wrong));
}

TEST_CASE("Account::is_login_barred gates on lock + active ban only",
          "[domain][identity]") {
    using domain::Ban;
    using domain::BanScope;
    const auto t0 = std::chrono::system_clock::time_point{} + std::chrono::hours{100};

    auto make_with = [](std::optional<Ban> ban, bool locked) {
        auto u = UserName::parse("Dave").value();
        auto h = BNHash::from_bytes(std::string(20, 0x42)).value();
        return Account::rehydrate(AccountId{7}, std::move(u), std::move(h),
                                  Locale::parse_or_default("enUS"),
                                  domain::identity::CommandGroupMask{},
                                  std::move(ban), locked);
    };

    const Ban expired{BanScope::Account, "old", AccountId{2},
                      t0 - std::chrono::hours{2}, t0 - std::chrono::hours{1}};
    const Ban active{BanScope::Account, "cur", AccountId{2},
                     t0, t0 + std::chrono::hours{1}};

    // An expired ban must NOT bar login -- kills the &&->|| mutant on
    // `ban_ && ban_->active_at(now)` (which would bar on any ban's presence).
    CHECK_FALSE(make_with(expired, /*locked=*/false).is_login_barred(t0));
    // An active ban bars.
    CHECK(make_with(active, /*locked=*/false).is_login_barred(t0));
    // A lock bars regardless of ban.
    CHECK(make_with(std::nullopt, /*locked=*/true).is_login_barred(t0));
    // Clean account is not barred.
    CHECK_FALSE(make_with(std::nullopt, /*locked=*/false).is_login_barred(t0));
}

TEST_CASE("Account::grant_command_group rejects out-of-range groups silently",
          "[domain][identity][permissions]") {
    // Kills the ||->&& mutant on the `group < 1 || group > kBits` guard: with
    // `&&` the guard never fires, so an out-of-range group would emit a
    // spurious AccountCommandGroupGranted event and leave no bit set.
    auto a = make_account("Gina", 0x66);
    (void)a.drain_events();
    a.grant_command_group(0);
    a.grant_command_group(9);
    auto evs = a.drain_events();
    CHECK(evs.empty());
    CHECK_FALSE(a.command_groups().has(0));
    CHECK_FALSE(a.command_groups().has(9));
    // A valid grant still works (and emits exactly one event).
    a.grant_command_group(2);
    auto evs2 = a.drain_events();
    REQUIRE(evs2.size() == 1);
    CHECK(std::holds_alternative<domain::events::AccountCommandGroupGranted>(evs2[0]));
    CHECK(a.command_groups().has(2));
}

TEST_CASE("Account::rehydrate preserves state without emitting events",
          "[domain][identity][repository]") {
    auto u = UserName::parse("Carol").value();
    auto h = BNHash::from_bytes(std::string(20, 0x55)).value();
    domain::identity::CommandGroupMask mask;
    mask.grant(3);
    auto a = Account::rehydrate(AccountId{42}, std::move(u), std::move(h),
                                Locale::parse_or_default("frFR"), mask,
                                std::nullopt, /*locked=*/false);
    REQUIRE(a.drain_events().empty());
    REQUIRE(a.id() == AccountId{42});
    REQUIRE(a.command_groups().has(3));
    REQUIRE(std::string{a.locale().text()} == "frFR");
}
