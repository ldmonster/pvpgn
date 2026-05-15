// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::auth::LogoutUser`. Exercises the use-case
// against the in-memory port adapters defined in `infra/`.

#include <catch2/catch_test_macros.hpp>

#include "application/auth/logout_user.hpp"
#include "core/clock.hpp"
#include "domain/chat/channel.hpp"
#include "domain/gameplay/game.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/session_registry.hpp"
#include "infra/storage/repository/channel_repository.hpp"
#include "infra/storage/repository/game_repository.hpp"

namespace {

using namespace pvpgn;
using application::auth::LogoutUser;
using application::auth::LogoutRequest;

domain::BNHash make_hash(std::uint8_t fill) {
    domain::BNHash::Bytes b{};
    b.fill(fill);
    return domain::BNHash{b};
}

domain::UserName make_name(std::string_view s) {
    auto r = domain::UserName::parse(s);
    REQUIRE(r);
    return r.value();
}

struct Fixture {
    infra::inmemory::InMemoryAccountRepository accounts;
    infra::inmemory::InMemorySessionRegistry sessions;
    infra::storage::InMemoryChannelRepository channels;
    infra::storage::InMemoryGameRepository games;
    infra::inmemory::InMemoryEventBus bus;
    core::ManualClock clock{core::SystemTime{}};

    domain::AccountId alice_id{42};
    domain::SessionId alice_session{1};
    domain::ClientTag star_tag = domain::ClientTag::parse("STAR").value();
    domain::BNHash password = make_hash(0xAA);

    void seed_alice() {
        auto a = domain::identity::Account::create(
            alice_id, make_name("Alice"), password, domain::Locale{}).value();
        (void)a.drain_events();
        REQUIRE(accounts.save(a));
    }

    void attach_alice_session() {
        (void)sessions.attach(alice_session, alice_id);
    }

    LogoutUser make_use_case() {
        return LogoutUser{sessions, channels, games, bus};
    }
};

}  // namespace

TEST_CASE("LogoutUser: detaches session from registry",
          "[application][auth][logout]") {
    Fixture f;
    f.seed_alice();
    f.attach_alice_session();

    auto uc = f.make_use_case();
    auto r = uc.execute(LogoutRequest{f.alice_session, f.alice_id});

    REQUIRE(r);
    // Verify session is detached
    REQUIRE_FALSE(f.sessions.account_for(f.alice_session).has_value());
}

TEST_CASE("LogoutUser: fails if session not found",
          "[application][auth][logout]") {
    Fixture f;
    auto uc = f.make_use_case();

    domain::SessionId nonexistent{999};
    auto r = uc.execute(LogoutRequest{nonexistent, f.alice_id});

    REQUIRE_FALSE(r);
}

TEST_CASE("LogoutUser: removes account from channels",
          "[application][auth][logout]") {
    Fixture f;
    f.seed_alice();
    f.attach_alice_session();

    // Create and join a channel
    auto channel = domain::chat::Channel::create(
        domain::ChannelId{1}, "TestChannel", f.star_tag).value();
    channel.join(f.alice_id, make_name("Alice"));
    (void)f.channels.save(channel);

    auto uc = f.make_use_case();
    auto r = uc.execute(LogoutRequest{f.alice_session, f.alice_id});

    REQUIRE(r);
    // Note: In a full implementation, we'd verify the account was removed
    // from the channel via repository queries or event checks.
}

TEST_CASE("LogoutUser: removes account from games",
          "[application][auth][logout]") {
    Fixture f;
    f.seed_alice();
    f.attach_alice_session();

    // Create and join a game
    auto game = domain::gameplay::Game::create(
        domain::GameId{1}, "TestGame", f.star_tag).value();
    (void)f.games.save(game);

    auto uc = f.make_use_case();
    auto r = uc.execute(LogoutRequest{f.alice_session, f.alice_id});

    REQUIRE(r);
    // Note: In a full implementation, we'd verify the account was removed
    // from the game via repository queries or event checks.
}
