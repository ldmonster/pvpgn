// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::social::DisbandTeam`.

#include <memory>
#include <unordered_map>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "application/social/disband_team.hpp"
#include "domain/social/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/team.hpp"
#include "infra/inmemory/event_bus.hpp"

namespace {

using namespace pvpgn;
using application::social::DisbandTeam;
using application::social::DisbandTeamError;

// ---------------------------------------------------------------------------
// Inline fake ITeamRepository
// ---------------------------------------------------------------------------
class FakeTeamRepository final : public application::ports::ITeamRepository {
public:
    core::Result<std::shared_ptr<domain::social::Team>, core::Error>
    find_by_id(domain::TeamId id) override {
        auto it = store_.find(id.value());
        if (it == store_.end()) {
            return core::fail(core::Error{core::StatusCode::NotFound, "team not found"});
        }
        return it->second;
    }

    core::Result<std::vector<std::shared_ptr<domain::social::Team>>, core::Error>
    find_by_member(domain::AccountId /*account_id*/) override {
        return std::vector<std::shared_ptr<domain::social::Team>>{};
    }

    core::Result<void, core::Error>
    save(const domain::social::Team& team) override {
        store_[team.id().value()] = std::make_shared<domain::social::Team>(team);
        return core::ok();
    }

    core::Result<void, core::Error>
    remove(domain::TeamId id) override {
        store_.erase(id.value());
        return core::ok();
    }

    bool contains(domain::TeamId id) const {
        return store_.count(id.value()) > 0;
    }

private:
    std::unordered_map<std::uint32_t, std::shared_ptr<domain::social::Team>> store_;
};

// Helper: create a team with two members and seed it into the fake repo.
domain::TeamId seed_team(FakeTeamRepository& repo,
                         domain::AccountId alice,
                         domain::AccountId bob) {
    domain::TeamId id{800};
    auto team_r = domain::social::Team::create(id, {alice, bob}, domain::ClientTag{});
    REQUIRE(team_r);
    REQUIRE(repo.save(team_r.value()));
    return id;
}

struct Fixture {
    std::shared_ptr<FakeTeamRepository>            teams =
        std::make_shared<FakeTeamRepository>();
    std::shared_ptr<infra::inmemory::InMemoryEventBus> bus =
        std::make_shared<infra::inmemory::InMemoryEventBus>();

    domain::AccountId alice{1};
    domain::AccountId bob{2};
    domain::AccountId outsider{99};

    DisbandTeam make_uc() { return DisbandTeam{teams, bus}; }
};

}  // namespace

TEST_CASE("DisbandTeam: member can disband the team",
          "[application][social][disband_team]") {
    Fixture f;
    auto team_id = seed_team(*f.teams, f.alice, f.bob);
    auto uc = f.make_uc();

    auto r = uc.execute(team_id, f.alice);

    REQUIRE(r);
    REQUIRE_FALSE(f.teams->contains(team_id));
}

TEST_CASE("DisbandTeam: second member can also disband the team",
          "[application][social][disband_team]") {
    Fixture f;
    auto team_id = seed_team(*f.teams, f.alice, f.bob);
    auto uc = f.make_uc();

    auto r = uc.execute(team_id, f.bob);

    REQUIRE(r);
    REQUIRE_FALSE(f.teams->contains(team_id));
}

TEST_CASE("DisbandTeam: unknown team returns TeamNotFound",
          "[application][social][disband_team]") {
    Fixture f;
    auto uc = f.make_uc();

    auto r = uc.execute(domain::TeamId{999}, f.alice);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == DisbandTeamError::TeamNotFound);
}

TEST_CASE("DisbandTeam: non-member returns NotAMember",
          "[application][social][disband_team]") {
    Fixture f;
    auto team_id = seed_team(*f.teams, f.alice, f.bob);
    auto uc = f.make_uc();

    auto r = uc.execute(team_id, f.outsider);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == DisbandTeamError::NotAMember);
}
