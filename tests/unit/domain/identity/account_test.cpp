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
