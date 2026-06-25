// SPDX-License-Identifier: GPL-2.0-or-later
//
// Regression tests for finding C-1: a user must be a member of at most ONE
// channel at a time. The original `conn_set_channel` (connection.cpp:1877)
// parts the current channel BEFORE joining the new one. `JoinChannel` must
// mirror that: joining B while in A drops the account from A (auto-deleting A
// if it is now empty and temporary), and re-joining the channel you are
// already in is an idempotent no-op.

#include <catch2/catch_test_macros.hpp>

#include "application/chat/join_channel.hpp"
#include "domain/chat/channel.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/session_registry.hpp"
#include "channel_repository.hpp"

namespace {

using namespace pvpgn;
using application::chat::JoinChannel;

domain::UserName make_name(std::string_view s) {
    auto r = domain::UserName::parse(s);
    REQUIRE(r);
    return r.value();
}

domain::BNHash make_hash(std::uint8_t fill) {
    domain::BNHash::Bytes b{};
    b.fill(fill);
    return domain::BNHash{b};
}

struct Fixture {
    infra::inmemory::InMemoryAccountRepository accounts;
    infra::storage::InMemoryChannelRepository channels;
    infra::inmemory::InMemorySessionRegistry sessions;

    domain::AccountId alice_id{1};
    domain::ClientTag star_tag = domain::ClientTag::parse("STAR").value();

    void seed_account(domain::AccountId id, std::string_view name) {
        auto a = domain::identity::Account::create(
            id, make_name(name), make_hash(0xAA),
            domain::Locale{}).value();
        (void)a.drain_events();
        REQUIRE(accounts.save(a));
    }

    JoinChannel make_use_case() {
        return JoinChannel{channels, accounts, sessions};
    }

    // Convenience: is `id` currently a member of the channel named `name`?
    bool member_of(std::string_view name, domain::AccountId id) {
        auto ch = channels.find_by_name(std::string{name});
        return ch && ch.value().contains(id);
    }
};

}  // namespace

TEST_CASE("JoinChannel: joining B while in A removes membership of A",
          "[application][chat][join][C-1]") {
    Fixture f;
    f.seed_account(f.alice_id, "Alice");
    auto uc = f.make_use_case();

    // Alice joins A (creating it), then joins B.
    auto rA = uc.execute(f.alice_id, "ChannelA", f.star_tag);
    REQUIRE(rA);
    REQUIRE(f.member_of("ChannelA", f.alice_id));

    auto rB = uc.execute(f.alice_id, "ChannelB", f.star_tag);
    REQUIRE(rB);

    // Member of B only.
    REQUIRE(rB.value().channel.contains(f.alice_id));
    REQUIRE(f.member_of("ChannelB", f.alice_id));

    // ChannelA is temporary and now empty → auto-deleted; Alice is no longer a
    // member of it under any circumstance.
    REQUIRE_FALSE(f.member_of("ChannelA", f.alice_id));
}

TEST_CASE("JoinChannel: leaving A empty auto-deletes a temporary channel",
          "[application][chat][join][C-1]") {
    Fixture f;
    f.seed_account(f.alice_id, "Alice");
    auto uc = f.make_use_case();

    auto rA = uc.execute(f.alice_id, "TempA", f.star_tag);
    REQUIRE(rA);
    REQUIRE(f.channels.size() == 1);

    // Move to B — TempA becomes empty and (being a temporary public channel)
    // must be removed from the repository entirely.
    auto rB = uc.execute(f.alice_id, "TempB", f.star_tag);
    REQUIRE(rB);

    REQUIRE_FALSE(f.channels.find_by_name("TempA"));
    REQUIRE(f.channels.find_by_name("TempB"));
    REQUIRE(f.channels.size() == 1);  // A gone, B present
}

TEST_CASE("JoinChannel: non-empty old channel survives the move",
          "[application][chat][join][C-1]") {
    Fixture f;
    f.seed_account(f.alice_id, "Alice");
    domain::AccountId bob_id{2};
    f.seed_account(bob_id, "Bob");
    auto uc = f.make_use_case();

    // Alice and Bob both in A.
    REQUIRE(uc.execute(f.alice_id, "ChannelA", f.star_tag));
    REQUIRE(uc.execute(bob_id, "ChannelA", f.star_tag));
    REQUIRE(f.member_of("ChannelA", f.alice_id));
    REQUIRE(f.member_of("ChannelA", bob_id));

    // Alice moves to B; A keeps Bob and is NOT deleted.
    REQUIRE(uc.execute(f.alice_id, "ChannelB", f.star_tag));

    REQUIRE_FALSE(f.member_of("ChannelA", f.alice_id));
    REQUIRE(f.member_of("ChannelA", bob_id));
    REQUIRE(f.member_of("ChannelB", f.alice_id));
}

TEST_CASE("JoinChannel: re-joining the SAME channel is idempotent",
          "[application][chat][join][C-1]") {
    Fixture f;
    f.seed_account(f.alice_id, "Alice");
    auto uc = f.make_use_case();

    auto r1 = uc.execute(f.alice_id, "OnlyChannel", f.star_tag);
    REQUIRE(r1);
    const auto id1 = r1.value().channel.id().value();

    // Joining the same channel again must NOT leave-then-rejoin (which would
    // briefly empty + auto-delete the temporary channel) and must not duplicate
    // the membership.
    auto r2 = uc.execute(f.alice_id, "OnlyChannel", f.star_tag);
    REQUIRE(r2);

    REQUIRE(r2.value().channel.id().value() == id1);   // same channel, not recreated
    REQUIRE(r2.value().channel.member_count() == 1);   // no duplicate member
    REQUIRE(f.channels.size() == 1);                   // not deleted+recreated
    REQUIRE(f.member_of("OnlyChannel", f.alice_id));
}

TEST_CASE("JoinChannel: case-insensitive same-channel re-join stays idempotent",
          "[application][chat][join][C-1]") {
    // The current channel is discovered by id, so even a differently-cased name
    // for the channel you are already in must resolve to the same channel and
    // NOT trigger a leave/auto-delete.
    Fixture f;
    f.seed_account(f.alice_id, "Alice");
    auto uc = f.make_use_case();

    auto r1 = uc.execute(f.alice_id, "War3", f.star_tag);
    REQUIRE(r1);
    const auto id1 = r1.value().channel.id().value();

    auto r2 = uc.execute(f.alice_id, "war3", f.star_tag);
    REQUIRE(r2);

    REQUIRE(r2.value().channel.id().value() == id1);
    REQUIRE(f.channels.size() == 1);
    REQUIRE(f.member_of("War3", f.alice_id));
}
