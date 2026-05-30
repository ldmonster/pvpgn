// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for CreateCharacterUseCase.

#include <catch2/catch_test_macros.hpp>

#include "application/realm/create_character.hpp"
#include "core/result.hpp"
#include "domain/realm/character.hpp"
#include "domain/realm/character_list.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pvpgn::application::realm {

// ---------------------------------------------------------------------------
// Inline fakes
// ---------------------------------------------------------------------------

class FakeCharacterRepository final : public ICharacterRepository {
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
        auto key = c.id().account_name + "/" + c.id().char_name;
        chars_.emplace(key, c);
        return core::Result<void, core::Error>();
    }

    core::Result<void, core::Error>
    remove(const domain::realm::CharacterId& id) override {
        auto key = id.account_name + "/" + id.char_name;
        chars_.erase(key);
        return core::Result<void, core::Error>();
    }

    core::Result<std::vector<domain::realm::Character>, core::Error>
    list_for_account(std::string_view account_name) override {
        std::vector<domain::realm::Character> result;
        for (auto& [k, v] : chars_) {
            if (v.id().account_name == account_name)
                result.push_back(v);
        }
        return core::Result<std::vector<domain::realm::Character>, core::Error>(
            std::move(result));
    }

    std::size_t size() const noexcept { return chars_.size(); }

private:
    std::unordered_map<std::string, domain::realm::Character> chars_;
};

class FakeCharacterListRepository final : public ICharacterListRepository {
public:
    core::Result<domain::realm::CharacterList, core::Error>
    find_for_account(std::string_view account_name) override {
        auto it = lists_.find(std::string{account_name});
        if (it == lists_.end()) {
            // Return empty list — not an error
            return core::Result<domain::realm::CharacterList, core::Error>(
                domain::realm::CharacterList{std::string{account_name}});
        }
        // CharacterList is non-copyable; rebuild from stored names
        domain::realm::CharacterList list{std::string{account_name}};
        for (auto& c : it->second) {
            (void)list.add(c);
        }
        return core::Result<domain::realm::CharacterList, core::Error>(std::move(list));
    }

    core::Result<void, core::Error>
    save(std::string_view account_name,
         const domain::realm::CharacterList& list) override {
        auto& stored = lists_[std::string{account_name}];
        stored.clear();
        for (auto* cp : list.list(domain::realm::SortMode::by_name)) {
            stored.push_back(*cp);
        }
        return core::Result<void, core::Error>();
    }

    std::size_t count(std::string_view account_name) const {
        auto it = lists_.find(std::string{account_name});
        return it == lists_.end() ? 0u : it->second.size();
    }

private:
    std::unordered_map<std::string,
                       std::vector<domain::realm::Character>> lists_;
};

}  // namespace pvpgn::application::realm

namespace pa = pvpgn::application::realm;

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("CreateCharacterUseCase: happy path creates character",
          "[application][realm][create_character]") {
    pa::FakeCharacterRepository  char_repo;
    pa::FakeCharacterListRepository list_repo;
    pa::CreateCharacterUseCase uc{char_repo, list_repo};

    pa::CreateCharacterCommand cmd;
    cmd.account_name = "alice";
    cmd.char_name    = "Sorceress";
    cmd.char_class   = pvpgn::domain::realm::CharacterClass::sorceress;

    auto result = uc.execute(cmd);
    REQUIRE(result.has_value());
    CHECK(char_repo.size() == 1);
    CHECK(list_repo.count("alice") == 1);
}

TEST_CASE("CreateCharacterUseCase: empty account_name returns InvalidArgument",
          "[application][realm][create_character]") {
    pa::FakeCharacterRepository  char_repo;
    pa::FakeCharacterListRepository list_repo;
    pa::CreateCharacterUseCase uc{char_repo, list_repo};

    pa::CreateCharacterCommand cmd;
    cmd.account_name = "";
    cmd.char_name    = "Paladin";

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::InvalidArgument);
}

TEST_CASE("CreateCharacterUseCase: empty char_name returns InvalidArgument",
          "[application][realm][create_character]") {
    pa::FakeCharacterRepository  char_repo;
    pa::FakeCharacterListRepository list_repo;
    pa::CreateCharacterUseCase uc{char_repo, list_repo};

    pa::CreateCharacterCommand cmd;
    cmd.account_name = "alice";
    cmd.char_name    = "";

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::InvalidArgument);
}

TEST_CASE("CreateCharacterUseCase: char_name > 15 chars returns InvalidArgument",
          "[application][realm][create_character]") {
    pa::FakeCharacterRepository  char_repo;
    pa::FakeCharacterListRepository list_repo;
    pa::CreateCharacterUseCase uc{char_repo, list_repo};

    pa::CreateCharacterCommand cmd;
    cmd.account_name = "alice";
    cmd.char_name    = "ThisNameIsTooLong";  // 17 chars

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::InvalidArgument);
}

TEST_CASE("CreateCharacterUseCase: duplicate char_name returns AlreadyExists",
          "[application][realm][create_character]") {
    pa::FakeCharacterRepository  char_repo;
    pa::FakeCharacterListRepository list_repo;
    pa::CreateCharacterUseCase uc{char_repo, list_repo};

    pa::CreateCharacterCommand cmd;
    cmd.account_name = "alice";
    cmd.char_name    = "Amazon";

    REQUIRE(uc.execute(cmd).has_value());

    // Second creation with same name
    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::AlreadyExists);
}
