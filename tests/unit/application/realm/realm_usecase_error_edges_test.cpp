// SPDX-License-Identifier: GPL-2.0-or-later
//
// Net-new error/edge coverage for four realm use-cases, filling the
// branches the per-use-case base tests skip:
//
//   join_game_server.cpp  - empty char_name guard (InvalidArgument)
//   load_character.cpp     - empty char_name guard + save-file-store load
//                            failure (store.load returns an error)
//   character_lock.cpp     - domain lock() rejection (already locked) and
//                            repo.save() failure on the lock/unlock paths
//   delete_character.cpp   - char-list find failure + char-list save failure
//
// All fakes are inline, mirroring the sibling *_test.cpp files in this dir.

#include <catch2/catch_test_macros.hpp>

#include "application/realm/character_lock.hpp"
#include "application/realm/delete_character.hpp"
#include "application/realm/join_game_server.hpp"
#include "application/realm/load_character.hpp"
#include "core/result.hpp"
#include "domain/realm/character.hpp"
#include "domain/realm/character_list.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pvpgn::application::realm {

// ---------------------------------------------------------------------------
// Character repository whose save() can be made to fail on demand.
// ---------------------------------------------------------------------------
class FailableCharRepo final : public ICharacterRepository {
public:
    bool fail_save = false;

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
        if (fail_save)
            return core::fail(core::make_error(core::StatusCode::Internal, "save fails"));
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
        chars_.insert_or_assign(c.id().account_name + "/" + c.id().char_name, std::move(c));
    }

private:
    std::unordered_map<std::string, domain::realm::Character> chars_;
};

// ---------------------------------------------------------------------------
// Save-file store whose load() always fails (character exists, save file does
// not / I/O error).
// ---------------------------------------------------------------------------
class FailingLoadStore final : public domain::realm::ISaveFileStore {
public:
    core::Result<std::vector<uint8_t>, core::Error>
    load(std::string_view, std::string_view) override {
        return core::fail(core::make_error(core::StatusCode::Internal, "load io error"));
    }
    core::Result<void, core::Error>
    store(std::string_view, std::string_view, std::span<const uint8_t>) override {
        return core::Result<void, core::Error>();
    }
    core::Result<bool, core::Error>
    exists(std::string_view, std::string_view) override {
        return core::Result<bool, core::Error>(false);
    }
    core::Result<void, core::Error>
    remove(std::string_view, std::string_view) override {
        return core::Result<void, core::Error>();
    }
};

// ---------------------------------------------------------------------------
// Character-list repository whose find/save can each be made to fail.
// ---------------------------------------------------------------------------
class FailableListRepo final : public ICharacterListRepository {
public:
    bool fail_find = false;
    bool fail_save = false;

    core::Result<domain::realm::CharacterList, core::Error>
    find_for_account(std::string_view account_name) override {
        if (fail_find)
            return core::fail(core::make_error(core::StatusCode::Internal, "find fails"));
        domain::realm::CharacterList list{std::string{account_name}};
        return core::Result<domain::realm::CharacterList, core::Error>(std::move(list));
    }
    core::Result<void, core::Error>
    save(std::string_view, const domain::realm::CharacterList&) override {
        if (fail_save)
            return core::fail(core::make_error(core::StatusCode::Internal, "save fails"));
        return core::Result<void, core::Error>();
    }
};

}  // namespace pvpgn::application::realm

namespace pa = pvpgn::application::realm;
namespace dr = pvpgn::domain::realm;

static dr::Character make_char(std::string account, std::string name,
                               bool locked = false,
                               std::string gs = "gs1.example.com") {
    dr::CharacterId id{std::move(account), std::move(name)};
    dr::CharacterStats stats;
    dr::Character c{std::move(id), stats, {}};
    if (locked)
        (void)c.lock(std::move(gs));
    return c;
}

// ---------------------------------------------------------------------------
// join_game_server
// ---------------------------------------------------------------------------

TEST_CASE("JoinGameServerUseCase: empty char_name returns InvalidArgument",
          "[application][realm][join_game_server][edges]") {
    pa::FailableCharRepo char_repo;
    pa::GameServerQueue  gs_queue;
    pa::JoinGameServerUseCase uc{char_repo, gs_queue};

    pa::JoinGameServerCommand cmd;
    cmd.account_name = "alice";
    cmd.char_name    = "";          // empty -> guarded
    cmd.game_name    = "TestGame";
    cmd.gs_address   = "gs1";

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::InvalidArgument);
}

// ---------------------------------------------------------------------------
// load_character
// ---------------------------------------------------------------------------

TEST_CASE("LoadCharacterUseCase: empty char_name returns InvalidArgument",
          "[application][realm][load_character][edges]") {
    pa::FailableCharRepo char_repo;
    pa::FailingLoadStore store;
    pa::LoadCharacterUseCase uc{char_repo, store};

    auto result = uc.execute({"alice", "", "gs1"});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::InvalidArgument);
}

TEST_CASE("LoadCharacterUseCase: save-file load failure is propagated",
          "[application][realm][load_character][edges]") {
    pa::FailableCharRepo char_repo;
    pa::FailingLoadStore store;
    char_repo.seed(make_char("alice", "Amazon"));   // character exists & unlocked

    pa::LoadCharacterUseCase uc{char_repo, store};
    auto result = uc.execute({"alice", "Amazon", "gs1.example.com"});

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::Internal);
}

// ---------------------------------------------------------------------------
// character_lock
// ---------------------------------------------------------------------------

TEST_CASE("CharacterLockUseCase: locking an already-locked character "
          "propagates the domain rejection",
          "[application][realm][character_lock][edges]") {
    pa::FailableCharRepo repo;
    repo.seed(make_char("alice", "Barb", /*locked=*/true, "gs1.example.com"));

    pa::CharacterLockUseCase uc{repo};
    // A different server tries to lock the already-locked character.
    pa::LockCharacterCommand cmd{"alice", "Barb", "gs2.example.com"};
    auto result = uc.lock(cmd);

    REQUIRE_FALSE(result);
    CHECK(result.error().code() == pvpgn::core::StatusCode::FailedPrecondition);
}

TEST_CASE("CharacterLockUseCase: repo save failure on lock is propagated",
          "[application][realm][character_lock][edges]") {
    pa::FailableCharRepo repo;
    repo.seed(make_char("alice", "Sorc"));   // unlocked -> domain lock() succeeds
    repo.fail_save = true;                    // ...but persistence fails

    pa::CharacterLockUseCase uc{repo};
    pa::LockCharacterCommand cmd{"alice", "Sorc", "gs1.example.com"};
    auto result = uc.lock(cmd);

    REQUIRE_FALSE(result);
    CHECK(result.error().code() == pvpgn::core::StatusCode::Internal);
}

TEST_CASE("CharacterLockUseCase: repo save failure on unlock is propagated",
          "[application][realm][character_lock][edges]") {
    pa::FailableCharRepo repo;
    repo.seed(make_char("alice", "Pala", /*locked=*/true, "gs1.example.com"));
    repo.fail_save = true;

    pa::CharacterLockUseCase uc{repo};
    pa::UnlockCharacterCommand cmd{"alice", "Pala", "gs1.example.com"};
    auto result = uc.unlock(cmd);

    REQUIRE_FALSE(result);
    CHECK(result.error().code() == pvpgn::core::StatusCode::Internal);
}

// ---------------------------------------------------------------------------
// delete_character
// ---------------------------------------------------------------------------

TEST_CASE("DeleteCharacterUseCase: character-list find failure is propagated",
          "[application][realm][delete_character][edges]") {
    pa::FailableCharRepo char_repo;
    pa::FailableListRepo list_repo;
    char_repo.seed(make_char("alice", "Amazon"));   // exists & unlocked
    list_repo.fail_find = true;

    pa::DeleteCharacterUseCase uc{char_repo, list_repo};
    auto result = uc.execute({"alice", "Amazon"});

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::Internal);
}

TEST_CASE("DeleteCharacterUseCase: character-list save failure is propagated",
          "[application][realm][delete_character][edges]") {
    pa::FailableCharRepo char_repo;
    pa::FailableListRepo list_repo;
    char_repo.seed(make_char("alice", "Amazon"));
    list_repo.fail_save = true;

    pa::DeleteCharacterUseCase uc{char_repo, list_repo};
    auto result = uc.execute({"alice", "Amazon"});

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::Internal);
}
