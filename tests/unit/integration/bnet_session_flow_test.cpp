// SPDX-License-Identifier: GPL-2.0-or-later
//
// Integration tests for BNet FSM + use-case round-trips.
// Golden tests that exercise the full session lifecycle.

#include <catch2/catch_test_macros.hpp>

#include "application/chat/join_channel.hpp"
#include "application/chat/leave_channel.hpp"
#include "application/chat/post_message.hpp"
#include "application/game/join_game.hpp"
#include "application/game/leave_game.hpp"
#include "application/game/start_game.hpp"
#include "domain/chat/channel.hpp"
#include "domain/gameplay/game.hpp"
#include "domain/shared/chat_message.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "channel_repository.hpp"
#include "game_repository.hpp"

namespace {

using namespace pvpgn;
using namespace pvpgn::application;

domain::UserName make_name(std::string_view s) {
    auto r = domain::UserName::parse(s);
    REQUIRE(r);
    return r.value();
}

domain::ChatMessage make_message(std::string_view text) {
    auto r = domain::ChatMessage::create(text);
    REQUIRE(r);
    return r.value();
}

/// Full-stack fixture for integration testing
struct BNetSessionFixture {
    infra::inmemory::InMemoryAccountRepository accounts;
    infra::storage::InMemoryChannelRepository channels;
    infra::storage::InMemoryGameRepository games;

    domain::AccountId alice_id{1};
    domain::AccountId bob_id{2};
    domain::ClientTag star_tag = domain::ClientTag::parse("STAR").value();

    void setup_accounts() {
        auto alice = domain::identity::Account::create(
            alice_id, make_name("Alice"),
            domain::BNHash::from_bytes(std::string(20, 0x00)).value(),
            domain::Locale{}).value();
        (void)alice.drain_events();
        REQUIRE(accounts.save(alice));

        auto bob = domain::identity::Account::create(
            bob_id, make_name("Bob"),
            domain::BNHash::from_bytes(std::string(20, 0x00)).value(),
            domain::Locale{}).value();
        (void)bob.drain_events();
        REQUIRE(accounts.save(bob));
    }
};

}  // namespace

TEST_CASE("BNet session: channel join and chat round-trip",
          "[integration][bnet][session]") {
    BNetSessionFixture f;
    f.setup_accounts();

    // Step 1: Alice joins a channel (simulating EnterChat response)
    auto join_uc = chat::JoinChannel{f.channels, f.accounts};
    auto join_r = join_uc.execute(f.alice_id, "Lobby", f.star_tag);
    REQUIRE(join_r);
    auto channel_id = join_r.value().channel.id();

    // Step 2: Bob joins the same channel
    auto bob_join = join_uc.execute(f.bob_id, "Lobby", f.star_tag);
    REQUIRE(bob_join);
    REQUIRE(bob_join.value().channel.id() == channel_id);

    // Step 3: Alice posts a message
    auto post_uc = chat::PostMessage{f.channels};
    auto msg = make_message("Hello everyone!");
    auto post_r = post_uc.execute(channel_id, f.alice_id, msg);
    REQUIRE(post_r);
    REQUIRE(!post_r.value().recipients.empty());

    // Step 4: Alice leaves the channel
    auto leave_uc = chat::LeaveChannel{f.channels};
    auto leave_r = leave_uc.execute(channel_id, f.alice_id);
    REQUIRE(leave_r);
    REQUIRE(!leave_r.value().channel_deleted);  // Bob is still there

    // Step 5: Bob leaves and channel is deleted
    auto bob_leave = leave_uc.execute(channel_id, f.bob_id);
    REQUIRE(bob_leave);
    REQUIRE(bob_leave.value().channel_deleted);  // Channel now empty and not permanent
    REQUIRE(f.channels.size() == 0);  // Channels have size() method
}

TEST_CASE("BNet session: game lifecycle round-trip",
          "[integration][bnet][session]") {
    BNetSessionFixture f;
    f.setup_accounts();

    // Step 1: Alice creates a game (StartAdvEx3)
    auto start_uc = game::StartGame{f.games};
    auto start_r = start_uc.execute(f.alice_id, f.star_tag, "TestGame", "Deathstar", 4);
    REQUIRE(start_r);
    auto game_id = start_r.value().game_id;
    REQUIRE(f.games.list_active().value().size() == 1);

    // Step 2: Bob joins the game
    auto join_uc = game::JoinGame{f.games};
    auto join_r = join_uc.execute(game_id, f.bob_id);
    REQUIRE(join_r);
    REQUIRE(!join_r.value().server_address.empty());
    // Note: server_port is populated by infrastructure layer, not application layer
    // In unit tests, it remains 0 (placeholder)
    REQUIRE(join_r.value().server_port == 0);

    // Step 3: Game has both players
    auto g = f.games.find_by_id(game_id.value());
    REQUIRE(g);
    REQUIRE(g.value()->players().size() == 2);

    // Step 4: Alice leaves (host migration)
    auto leave_uc = game::LeaveGame{f.games};
    auto leave_r = leave_uc.execute(game_id, f.alice_id);
    REQUIRE(leave_r);
    REQUIRE(leave_r.value().host_migrated);
    REQUIRE(leave_r.value().new_host == f.bob_id);

    // Step 5: Bob leaves and game is cleaned up
    auto bob_leave = leave_uc.execute(game_id, f.bob_id);
    REQUIRE(bob_leave);
    REQUIRE(bob_leave.value().game_deleted);
    REQUIRE(f.games.list_active().value().size() == 0);
}

TEST_CASE("BNet session: full chat + game integration",
          "[integration][bnet][session]") {
    BNetSessionFixture f;
    f.setup_accounts();

    // Chat phase: join channel
    auto join_ch = chat::JoinChannel{f.channels, f.accounts};
    auto ch_r = join_ch.execute(f.alice_id, "Game Lobby", f.star_tag);
    REQUIRE(ch_r);
    auto channel_id = ch_r.value().channel.id();

    // Bob joins chat
    (void)join_ch.execute(f.bob_id, "Game Lobby", f.star_tag);

    // Post message in chat
    auto post_msg = chat::PostMessage{f.channels};
    (void)post_msg.execute(channel_id, f.alice_id, make_message("Starting game!"));

    // Game phase: Alice creates game
    auto start_game = game::StartGame{f.games};
    auto game_r = start_game.execute(f.alice_id, f.star_tag, "MyGame", "Deathstar", 2);
    REQUIRE(game_r);
    auto game_id = game_r.value().game_id;

    // Bob joins game
    auto join_game = game::JoinGame{f.games};
    (void)join_game.execute(game_id, f.bob_id);

    // Verify both channel and game exist
    REQUIRE(f.channels.size() == 1);
    REQUIRE(f.games.list_active().value().size() == 1);

    // Clean up: leave game
    auto leave_game = game::LeaveGame{f.games};
    (void)leave_game.execute(game_id, f.alice_id);
    (void)leave_game.execute(game_id, f.bob_id);

    // Clean up: leave channel
    auto leave_ch = chat::LeaveChannel{f.channels};
    (void)leave_ch.execute(channel_id, f.alice_id);
    (void)leave_ch.execute(channel_id, f.bob_id);

    // Verify both are cleaned up
    REQUIRE(f.channels.size() == 0);
    REQUIRE(f.games.list_active().value().size() == 0);
}
