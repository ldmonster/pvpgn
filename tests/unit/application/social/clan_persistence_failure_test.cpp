// SPDX-License-Identifier: GPL-2.0-or-later
//
// PersistenceFailed outcome tests for the clan write use-cases. The in-memory
// clan repository never fails its save()/remove(), so these arms are otherwise
// unreachable. We wrap a working InMemoryClanRepository in a decorator whose
// writes fail, while reads (find_by_id / find_by_tag) still resolve, so the
// use-case reaches the post-domain persistence step and reports the error.

#include <memory>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "application/social/disband_clan.hpp"
#include "application/social/invite_to_clan.hpp"
#include "application/social/join_clan.hpp"
#include "application/social/kick_from_clan.hpp"
#include "application/social/leave_clan.hpp"
#include "application/social/promote_clan_member.hpp"
#include "application/social/set_clan_motd.hpp"
#include "domain/social/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/clan.hpp"
#include "infra/inmemory/clan_repository.hpp"
#include "infra/inmemory/event_bus.hpp"

namespace {

using namespace pvpgn;

// IClanRepository decorator: reads delegate to a real in-memory repo; writes
// (save / remove) always fail. Lets us drive the use-cases past their domain
// guards into the persistence-failure branch.
class FailingWriteClanRepository final
    : public domain::social::IClanRepository {
public:
    explicit FailingWriteClanRepository(
        std::shared_ptr<infra::inmemory::InMemoryClanRepository> inner)
        : inner_(std::move(inner)) {}

    core::Result<std::shared_ptr<domain::social::Clan>, core::Error>
    find_by_id(domain::ClanId id) override { return inner_->find_by_id(id); }

    core::Result<std::shared_ptr<domain::social::Clan>, core::Error>
    find_by_tag(std::string_view tag) override {
        return inner_->find_by_tag(tag);
    }

    core::Result<std::shared_ptr<domain::social::Clan>, core::Error>
    find_by_name(std::string_view name) override {
        return inner_->find_by_name(name);
    }

    core::Result<void, core::Error>
    save(const domain::social::Clan& /*clan*/) override {
        return core::fail(
            core::Error{core::StatusCode::Internal, "save failed (test)"});
    }

    core::Result<void, core::Error>
    remove(std::string_view /*tag*/) override {
        return core::fail(
            core::Error{core::StatusCode::Internal, "remove failed (test)"});
    }

private:
    std::shared_ptr<infra::inmemory::InMemoryClanRepository> inner_;
};

struct Fixture {
    std::shared_ptr<infra::inmemory::InMemoryClanRepository> inner =
        std::make_shared<infra::inmemory::InMemoryClanRepository>();
    std::shared_ptr<FailingWriteClanRepository> clans =
        std::make_shared<FailingWriteClanRepository>(inner);
    std::shared_ptr<infra::inmemory::InMemoryEventBus> bus =
        std::make_shared<infra::inmemory::InMemoryEventBus>();

    domain::AccountId chieftain{1};
    domain::AccountId member{2};

    // Seed a clan (chieftain + a grunt member) into the inner repo so reads
    // succeed regardless of the failing decorator's writes.
    domain::ClanId seed() {
        domain::ClanId id{800};
        auto clan_r = domain::social::Clan::create(
            id, "PER", "Persist Clan", chieftain, domain::ClientTag{});
        REQUIRE(clan_r);
        clan_r.value().join(member, domain::social::ClanRank::Grunt);
        REQUIRE(inner->save(clan_r.value()));
        return id;
    }
};

}  // namespace

TEST_CASE("JoinClan: save failure returns PersistenceFailed",
          "[application][social][join_clan]") {
    Fixture f;
    auto clan_id = f.seed();
    application::social::JoinClan uc{f.clans, f.bus};

    auto r = uc.execute(clan_id, domain::AccountId{3});

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == application::social::JoinClanError::PersistenceFailed);
}

TEST_CASE("InviteToClan: save failure returns PersistenceFailed",
          "[application][social][invite_to_clan]") {
    Fixture f;
    auto clan_id = f.seed();
    application::social::InviteToClan uc{f.clans, f.bus};

    auto r = uc.execute(clan_id, f.chieftain, domain::AccountId{3});

    REQUIRE_FALSE(r);
    REQUIRE(r.error() ==
            application::social::InviteToClanError::PersistenceFailed);
}

TEST_CASE("PromoteClanMember: save failure returns PersistenceFailed",
          "[application][social][promote_clan_member]") {
    Fixture f;
    auto clan_id = f.seed();
    application::social::PromoteClanMember uc{f.clans, f.bus};

    auto r = uc.execute(clan_id, f.chieftain, f.member, "shaman");

    REQUIRE_FALSE(r);
    REQUIRE(r.error() ==
            application::social::PromoteClanMemberError::PersistenceFailed);
}

TEST_CASE("KickFromClan: save failure returns PersistenceFailed",
          "[application][social][kick_from_clan]") {
    Fixture f;
    auto clan_id = f.seed();
    application::social::KickFromClan uc{f.clans, f.bus};

    auto r = uc.execute(clan_id, f.chieftain, f.member);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() ==
            application::social::KickFromClanError::PersistenceFailed);
}

TEST_CASE("LeaveClan: save failure returns PersistenceFailed",
          "[application][social][leave_clan]") {
    Fixture f;
    auto clan_id = f.seed();
    application::social::LeaveClan uc{f.clans, f.bus};

    // The grunt member leaving is authorized; only the save fails.
    auto r = uc.execute(clan_id, f.member);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == application::social::LeaveClanError::PersistenceFailed);
}

TEST_CASE("SetClanMotd: save failure returns PersistenceFailed",
          "[application][social][set_clan_motd]") {
    Fixture f;
    auto clan_id = f.seed();
    application::social::SetClanMotd uc{f.clans, f.bus};

    auto r = uc.execute(clan_id, f.chieftain, "a brand new motd");

    REQUIRE_FALSE(r);
    REQUIRE(r.error() ==
            application::social::SetClanMotdError::PersistenceFailed);
}

TEST_CASE("DisbandClan: remove failure returns PersistenceFailed",
          "[application][social][disband_clan]") {
    Fixture f;
    auto clan_id = f.seed();
    application::social::DisbandClan uc{f.clans, f.bus};

    auto r = uc.execute(clan_id, f.chieftain);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() ==
            application::social::DisbandClanError::PersistenceFailed);
}
