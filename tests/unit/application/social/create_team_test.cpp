// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::social::CreateTeam`.

#include <memory>
#include <unordered_map>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "application/social/create_team.hpp"
#include "domain/social/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/team.hpp"
#include "infra/inmemory/event_bus.hpp"

namespace {

using namespace pvpgn;
using application::social::CreateTeam;
using application::social::CreateTeamError;
using application::social::CreateTeamRequest;

// ---------------------------------------------------------------------------
// Inline fake ITeamRepository
// ---------------------------------------------------------------------------
class FakeTeamRepository final : public domain::social::ITeamRepository {
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

    std::size_t size() const { return store_.size(); }

private:
    std::unordered_map<std::uint32_t, std::shared_ptr<domain::social::Team>> store_;
};

struct Fixture {
    std::shared_ptr<FakeTeamRepository>            teams =
        std::make_shared<FakeTeamRepository>();
    std::shared_ptr<infra::inmemory::InMemoryEventBus> bus =
        std::make_shared<infra::inmemory::InMemoryEventBus>();

    domain::AccountId alice{1};
    domain::AccountId bob{2};
    domain::AccountId carol{3};

    CreateTeam make_uc() { return CreateTeam{teams, bus}; }
};

}  // namespace

TEST_CASE("CreateTeam: two members creates a team and returns TeamInfo",
          "[application][social][create_team]") {
    Fixture f;
    auto uc = f.make_uc();

    CreateTeamRequest req{f.alice, {f.alice, f.bob}};
    auto r = uc.execute(req);

    REQUIRE(r);
    REQUIRE(r.value().members.size() == 2);
    REQUIRE(f.teams->size() == 1);
}

TEST_CASE("CreateTeam: three members creates a team",
          "[application][social][create_team]") {
    Fixture f;
    auto uc = f.make_uc();

    CreateTeamRequest req{f.alice, {f.alice, f.bob, f.carol}};
    auto r = uc.execute(req);

    REQUIRE(r);
    REQUIRE(r.value().members.size() == 3);
}

TEST_CASE("CreateTeam: single member returns InvalidMemberCount",
          "[application][social][create_team]") {
    Fixture f;
    auto uc = f.make_uc();

    CreateTeamRequest req{f.alice, {f.alice}};
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == CreateTeamError::InvalidMemberCount);
}

TEST_CASE("CreateTeam: five members returns InvalidMemberCount",
          "[application][social][create_team]") {
    Fixture f;
    auto uc = f.make_uc();

    CreateTeamRequest req{f.alice,
                          {f.alice, f.bob, f.carol,
                           domain::AccountId{4}, domain::AccountId{5}}};
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == CreateTeamError::InvalidMemberCount);
}

TEST_CASE("CreateTeam: duplicate members returns DuplicateMembers",
          "[application][social][create_team]") {
    Fixture f;
    auto uc = f.make_uc();

    CreateTeamRequest req{f.alice, {f.alice, f.alice}};
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == CreateTeamError::DuplicateMembers);
}
