// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "application/anongame_lobby/lobby.hpp"
#include "application/anongame_lobby/lobby_repository.hpp"

#include <span>
#include <vector>

namespace pal = pvpgn::application::anongame_lobby;

namespace {

pal::LobbyEntry make_entry(std::uint32_t account_id,
                           std::uint32_t client_tag = 0x57415233u,  // WAR3
                           std::uint32_t game_type  = 1u,
                           std::uint16_t skill      = 1000u) {
    return pal::LobbyEntry{account_id, client_tag, game_type, skill};
}

pal::LobbyAdmitRequest make_req(pal::LobbyEntry entrant,
                                std::span<const pal::LobbyEntry> queue,
                                std::uint8_t bracket_size) {
    pal::LobbyAdmitRequest r;
    r.entrant       = entrant;
    r.current_queue = queue;
    r.bracket_size  = bracket_size;
    return r;
}

// In-memory test double for IAnonGameLobbyRepository, exercised
// to confirm the interface compiles + can drive a simple
// admit/promote cycle. We do NOT depend on this in the dispatcher
// tests (dispatcher is span-based) -- this just protects the
// interface shape for future bridge wiring.
class StaticLobbyRepository final : public pal::IAnonGameLobbyRepository {
public:
    StaticLobbyRepository(std::uint8_t bracket,
                          std::vector<pal::LobbyEntry> initial = {})
        : bracket_(bracket), queue_(std::move(initial)) {}

    std::vector<pal::LobbyEntry>
    queue_for(std::uint32_t) const override {
        return queue_;
    }

    std::uint8_t bracket_size_for(std::uint32_t) const override {
        return bracket_;
    }

    void add(pal::LobbyEntry const& e) override {
        queue_.push_back(e);
    }

    void remove_party(std::span<const pal::LobbyEntry> party) override {
        for (auto const& p : party) {
            auto it = std::find_if(queue_.begin(), queue_.end(),
                [&](pal::LobbyEntry const& q) {
                    return q.account_id == p.account_id;
                });
            if (it != queue_.end()) queue_.erase(it);
        }
    }

    std::size_t size() const { return queue_.size(); }

private:
    std::uint8_t                 bracket_;
    std::vector<pal::LobbyEntry> queue_;
};

}  // namespace

TEST_CASE("anongame_lobby: bracket_size<2 -> kRejected", "[application][anongame_lobby]") {
    auto req = make_req(make_entry(42), {}, 0);
    auto r = pal::dispatch_admit(req);
    CHECK(r.status == pal::LobbyAdmitStatus::kRejected);
    CHECK(r.promoted_party.empty());

    req.bracket_size = 1;
    r = pal::dispatch_admit(req);
    CHECK(r.status == pal::LobbyAdmitStatus::kRejected);
}

TEST_CASE("anongame_lobby: entrant.account_id==0 -> kRejected", "[application][anongame_lobby]") {
    auto req = make_req(make_entry(0), {}, 2);
    auto r = pal::dispatch_admit(req);
    CHECK(r.status == pal::LobbyAdmitStatus::kRejected);
}

TEST_CASE("anongame_lobby: empty queue + bracket=2 -> kQueued", "[application][anongame_lobby]") {
    auto req = make_req(make_entry(42), {}, 2);
    auto r = pal::dispatch_admit(req);
    CHECK(r.status == pal::LobbyAdmitStatus::kQueued);
    CHECK(r.promoted_party.empty());
}

TEST_CASE("anongame_lobby: existing queue of 1 + bracket=2 -> kPromoted with both members", "[application][anongame_lobby]") {
    std::vector<pal::LobbyEntry> q{make_entry(1)};
    auto req = make_req(make_entry(42), q, 2);
    auto r = pal::dispatch_admit(req);
    REQUIRE(r.status == pal::LobbyAdmitStatus::kPromoted);
    REQUIRE(r.promoted_party.size() == 2);
    CHECK(r.promoted_party[0].account_id == 1u);
    CHECK(r.promoted_party[1].account_id == 42u);
}

TEST_CASE("anongame_lobby: kDuplicate when entrant.account_id is already in the queue", "[application][anongame_lobby]") {
    std::vector<pal::LobbyEntry> q{make_entry(42)};
    auto req = make_req(make_entry(42), q, 2);
    auto r = pal::dispatch_admit(req);
    CHECK(r.status == pal::LobbyAdmitStatus::kDuplicate);
    CHECK(r.promoted_party.empty());
}

TEST_CASE("anongame_lobby: bracket=4 fills FIFO -> kQueued / kQueued / kQueued / kPromoted", "[application][anongame_lobby]") {
    StaticLobbyRepository repo(4);

    // First three entrants queue.
    for (std::uint32_t i = 1; i <= 3; ++i) {
        auto queue = repo.queue_for(0);
        auto req   = make_req(make_entry(i), queue, repo.bracket_size_for(0));
        auto r     = pal::dispatch_admit(req);
        CHECK(r.status == pal::LobbyAdmitStatus::kQueued);
        repo.add(make_entry(i));
    }
    CHECK(repo.size() == 3);

    // Fourth entrant promotes the bracket.
    auto queue = repo.queue_for(0);
    auto req   = make_req(make_entry(4), queue, repo.bracket_size_for(0));
    auto r     = pal::dispatch_admit(req);
    REQUIRE(r.status == pal::LobbyAdmitStatus::kPromoted);
    REQUIRE(r.promoted_party.size() == 4);
    CHECK(r.promoted_party[0].account_id == 1u);
    CHECK(r.promoted_party[1].account_id == 2u);
    CHECK(r.promoted_party[2].account_id == 3u);
    CHECK(r.promoted_party[3].account_id == 4u);

    repo.remove_party(r.promoted_party);
    CHECK(repo.size() == 0);
}

TEST_CASE("anongame_lobby: overflow queue >= bracket -> kRejected", "[application][anongame_lobby]") {
    std::vector<pal::LobbyEntry> q{make_entry(1), make_entry(2), make_entry(3)};
    auto req = make_req(make_entry(42), q, 2);  // queue already 3, bracket 2
    auto r = pal::dispatch_admit(req);
    CHECK(r.status == pal::LobbyAdmitStatus::kRejected);
}

TEST_CASE("anongame_lobby: bracket=8 with 7 waiting -> kPromoted preserving full party", "[application][anongame_lobby]") {
    std::vector<pal::LobbyEntry> q;
    for (std::uint32_t i = 1; i <= 7; ++i) q.push_back(make_entry(i));
    auto req = make_req(make_entry(99), q, 8);
    auto r = pal::dispatch_admit(req);
    REQUIRE(r.status == pal::LobbyAdmitStatus::kPromoted);
    REQUIRE(r.promoted_party.size() == 8);
    for (std::uint32_t i = 0; i < 7; ++i) {
        CHECK(r.promoted_party[i].account_id == i + 1u);
    }
    CHECK(r.promoted_party[7].account_id == 99u);
}

TEST_CASE("anongame_lobby: promoted_party preserves entrant metadata", "[application][anongame_lobby]") {
    std::vector<pal::LobbyEntry> q{make_entry(1, 0x57333050u, 5u, 1234u)};  // W3XP
    auto entrant = make_entry(42, 0x57415233u, 5u, 4321u);                  // WAR3
    auto req = make_req(entrant, q, 2);
    auto r = pal::dispatch_admit(req);
    REQUIRE(r.status == pal::LobbyAdmitStatus::kPromoted);
    REQUIRE(r.promoted_party.size() == 2);
    CHECK(r.promoted_party[0].client_tag  == 0x57333050u);
    CHECK(r.promoted_party[0].skill_level == 1234u);
    CHECK(r.promoted_party[1].client_tag  == 0x57415233u);
    CHECK(r.promoted_party[1].skill_level == 4321u);
}
