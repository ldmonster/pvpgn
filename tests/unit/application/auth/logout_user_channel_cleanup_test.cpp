// SPDX-License-Identifier: GPL-2.0-or-later
//
// Net-new coverage for `application::auth::LogoutUser`. The base
// logout_user_test.cpp drives success, session-not-found, and the
// nullptr-`leave_channel_` channel/game seeds (which never enter the
// cleanup loop). This file covers the otherwise-uncovered R305 branch:
// a non-null LeaveChannel use-case that actually iterates the channel
// repository and removes the account's memberships on disconnect.
// It also pins the double-logout (second logout fails NotFound) path.

#include <catch2/catch_test_macros.hpp>

#include "application/auth/logout_user.hpp"
#include "application/chat/leave_channel.hpp"
#include "core/clock.hpp"
#include "domain/chat/channel.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/channel_repository.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/game_repository.hpp"
#include "infra/inmemory/session_registry.hpp"

namespace {

using namespace pvpgn;
using application::auth::LogoutRequest;
using application::auth::LogoutUser;
using application::chat::LeaveChannel;

[[maybe_unused]] domain::UserName make_name(std::string_view s) {
    auto r = domain::UserName::parse(s);
    REQUIRE(r);
    return r.value();
}

struct Fixture {
    infra::inmemory::InMemoryAccountRepository accounts;
    infra::inmemory::InMemorySessionRegistry   sessions;
    infra::inmemory::InMemoryChannelRepository channels;
    infra::inmemory::InMemoryGameRepository    games;
    infra::inmemory::InMemoryEventBus          bus;

    domain::AccountId alice_id{42};
    domain::SessionId alice_session{1};
    domain::ClientTag star = domain::ClientTag::parse("STAR").value();

    void attach_alice() {
        (void)sessions.attach(alice_session, alice_id);
    }

    // Create a channel containing Alice and persist it.
    domain::ChannelId join_channel(std::uint32_t cid) {
        auto channel = domain::chat::Channel::create(
            domain::ChannelId{cid}, "TestChannel",
            domain::chat::ChannelPolicy{
                .flags = domain::chat::ChannelFlags{}, .client = star});
        (void)channel.admit(alice_id, star);
        (void)channels.save(channel);
        return domain::ChannelId{cid};
    }
};

}  // namespace

TEST_CASE("LogoutUser: with LeaveChannel injected, removes the account "
          "from a joined channel",
          "[application][auth][logout][channel_cleanup]") {
    Fixture f;
    f.attach_alice();
    auto cid = f.join_channel(1);

    // Sanity: Alice is in the channel before logout.
    {
        auto before = f.channels.find_by_id(cid);
        REQUIRE(before);
        REQUIRE(before.value().contains(f.alice_id));
    }

    LeaveChannel leave{f.channels, f.sessions};
    LogoutUser uc{f.sessions, f.channels, f.games, f.bus, &leave};

    auto r = uc.execute(LogoutRequest{f.alice_session, f.alice_id});
    REQUIRE(r);

    // The membership must have been removed by the cleanup loop.
    // (The channel may also have been deleted if it became empty + non-
    // permanent, which is equally acceptable for this assertion.)
    auto after = f.channels.find_by_id(cid);
    if (after) {
        CHECK_FALSE(after.value().contains(f.alice_id));
    }
    // Session is detached regardless.
    CHECK_FALSE(f.sessions.account_for(f.alice_session).has_value());
}

TEST_CASE("LogoutUser: cleanup loop tolerates an account in no channels",
          "[application][auth][logout][channel_cleanup]") {
    Fixture f;
    f.attach_alice();
    // A channel that does NOT contain Alice — the predicate skips it.
    auto channel = domain::chat::Channel::create(
        domain::ChannelId{9}, "Empty",
        domain::chat::ChannelPolicy{
            .flags = domain::chat::ChannelFlags{}, .client = f.star});
    (void)f.channels.save(channel);

    LeaveChannel leave{f.channels, f.sessions};
    LogoutUser uc{f.sessions, f.channels, f.games, f.bus, &leave};

    auto r = uc.execute(LogoutRequest{f.alice_session, f.alice_id});
    REQUIRE(r);
    CHECK_FALSE(f.sessions.account_for(f.alice_session).has_value());
}

TEST_CASE("LogoutUser: double logout — the second call fails NotFound",
          "[application][auth][logout]") {
    Fixture f;
    f.attach_alice();

    LeaveChannel leave{f.channels, f.sessions};
    LogoutUser uc{f.sessions, f.channels, f.games, f.bus, &leave};

    auto first = uc.execute(LogoutRequest{f.alice_session, f.alice_id});
    REQUIRE(first);

    // Session already gone — the existence guard rejects the repeat.
    auto second = uc.execute(LogoutRequest{f.alice_session, f.alice_id});
    REQUIRE_FALSE(second);
    CHECK(second.error().code() == core::StatusCode::NotFound);
}
