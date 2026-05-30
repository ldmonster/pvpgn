// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for JoinGameServerUseCase.

#include <catch2/catch_test_macros.hpp>

#include "application/realm/join_game_server.hpp"
#include "core/result.hpp"
#include "domain/realm/character.hpp"

#include <chrono>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pvpgn::application::realm {

class FakeCharRepo6 final : public ICharacterRepository {
public:
    core::Result<domain::realm::Character, core::Error>
    find(const domain::realm::CharacterId& id) override {
        auto key = id.account_name + "/" + id.char_name;
        auto it  = chars_.find(key);
        if (it == chars_.end())
            return core::fail(core::make_error(core::StatusCode::NotFound, "not found"));
        return core::Result<domain::realm::Character, core::Error>(it->second);
    }

    core::Result<void, core::Error>
    save(const domain::realm::Character& c) override {
        chars_.insert_or_assign(c.id().account_name + "/" + c.id().char_name, c);
        return core::Result<void, core::Error>();
    }

    core::Result<void, core::Error>
    remove(const domain::realm::CharacterId& id) override {
        chars_.erase(id.account_name + "/" + id.char_name);
        return core::Result<void, core::Error>();
    }

    core::Result<std::vector<domain::realm::Character>, core::Error>
    list_for_account(std::string_view account_name) override {
        std::vector<domain::realm::Character> result;
        for (auto& [k, v] : chars_)
            if (v.id().account_name == account_name)
                result.push_back(v);
        return core::Result<std::vector<domain::realm::Character>, core::Error>(
            std::move(result));
    }

    void seed(domain::realm::Character c) {
        chars_[c.id().account_name + "/" + c.id().char_name] = std::move(c);
    }

private:
    std::unordered_map<std::string, domain::realm::Character> chars_;
};

}  // namespace pvpgn::application::realm

namespace pa = pvpgn::application::realm;
namespace dr = pvpgn::domain::realm;

static dr::Character make_char6(std::string account, std::string name,
                                  bool locked = false) {
    dr::CharacterId id{std::move(account), std::move(name)};
    dr::CharacterStats stats;
    dr::Character c{std::move(id), stats, {}};
    if (locked)
        (void)c.lock("gs1.example.com");
    return c;
}

static pa::GameServerInfo make_gs(std::string address, uint16_t port = 4000) {
    pa::GameServerInfo gs;
    gs.address        = std::move(address);
    gs.port           = port;
    gs.status         = pa::GameServerStatus::online;
    gs.current_players = 0;
    gs.max_players    = 255;
    gs.last_heartbeat = std::chrono::system_clock::now();
    return gs;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("JoinGameServerUseCase: happy path returns token and address",
          "[application][realm][join_game_server]") {
    pa::FakeCharRepo6 char_repo;
    pa::GameServerQueue gs_queue;

    char_repo.seed(make_char6("alice", "Amazon"));
    gs_queue.register_server(make_gs("gs1.example.com", 4000));

    pa::JoinGameServerUseCase uc{char_repo, gs_queue};
    pa::JoinGameServerCommand cmd;
    cmd.account_name = "alice";
    cmd.char_name    = "Amazon";
    cmd.game_name    = "TestGame";
    cmd.game_pass    = "";
    cmd.gs_address   = "gs1.example.com";

    auto result = uc.execute(cmd);
    REQUIRE(result.has_value());
    CHECK(result.value().game_token != 0);
    CHECK(result.value().gs_address == "gs1.example.com");
    CHECK(result.value().gs_port == 4000);
}

TEST_CASE("JoinGameServerUseCase: empty account_name returns InvalidArgument",
          "[application][realm][join_game_server]") {
    pa::FakeCharRepo6 char_repo;
    pa::GameServerQueue gs_queue;
    pa::JoinGameServerUseCase uc{char_repo, gs_queue};

    pa::JoinGameServerCommand cmd;
    cmd.account_name = "";
    cmd.char_name    = "Amazon";
    cmd.game_name    = "TestGame";
    cmd.gs_address   = "gs1";

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::InvalidArgument);
}

TEST_CASE("JoinGameServerUseCase: non-existent character returns NotFound",
          "[application][realm][join_game_server]") {
    pa::FakeCharRepo6 char_repo;
    pa::GameServerQueue gs_queue;
    gs_queue.register_server(make_gs("gs1.example.com"));

    pa::JoinGameServerUseCase uc{char_repo, gs_queue};
    pa::JoinGameServerCommand cmd;
    cmd.account_name = "alice";
    cmd.char_name    = "Ghost";
    cmd.game_name    = "TestGame";
    cmd.gs_address   = "gs1.example.com";

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::NotFound);
}

TEST_CASE("JoinGameServerUseCase: already-locked character returns FailedPrecondition",
          "[application][realm][join_game_server]") {
    pa::FakeCharRepo6 char_repo;
    pa::GameServerQueue gs_queue;

    char_repo.seed(make_char6("alice", "Barb", /*locked=*/true));
    gs_queue.register_server(make_gs("gs1.example.com"));

    pa::JoinGameServerUseCase uc{char_repo, gs_queue};
    pa::JoinGameServerCommand cmd;
    cmd.account_name = "alice";
    cmd.char_name    = "Barb";
    cmd.game_name    = "TestGame";
    cmd.gs_address   = "gs1.example.com";

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::FailedPrecondition);
}

TEST_CASE("JoinGameServerUseCase: unknown game server returns NotFound",
          "[application][realm][join_game_server]") {
    pa::FakeCharRepo6 char_repo;
    pa::GameServerQueue gs_queue;

    char_repo.seed(make_char6("alice", "Necro"));
    // gs_queue has no servers registered

    pa::JoinGameServerUseCase uc{char_repo, gs_queue};
    pa::JoinGameServerCommand cmd;
    cmd.account_name = "alice";
    cmd.char_name    = "Necro";
    cmd.game_name    = "TestGame";
    cmd.gs_address   = "gs99.example.com";

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::NotFound);
}
