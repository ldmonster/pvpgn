// SPDX-License-Identifier: GPL-2.0-or-later
//
// Edge-case unit tests for CreateCharacterUseCase (create_character.cpp).
// Complements create_character_test.cpp by exercising the capacity-exhausted
// branch and the persistence-failure propagation branches.

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

// In-memory character repository that can be switched into a failing mode.
class FailingCharRepo final : public ICharacterRepository {
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
            return core::fail(core::make_error(core::StatusCode::Internal, "save boom"));
        chars_.emplace(c.id().account_name + "/" + c.id().char_name, c);
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

private:
    std::unordered_map<std::string, domain::realm::Character> chars_;
};

// Character-list repository with a configurable preset roster and failure modes.
class ConfigurableListRepo final : public ICharacterListRepository {
public:
    bool        fail_find = false;
    bool        fail_save = false;
    std::size_t preset    = 0;  // number of characters returned for any account

    core::Result<domain::realm::CharacterList, core::Error>
    find_for_account(std::string_view account_name) override {
        if (fail_find)
            return core::fail(core::make_error(core::StatusCode::Internal, "find boom"));
        domain::realm::CharacterList list{std::string{account_name}};
        for (std::size_t i = 0; i < preset; ++i) {
            domain::realm::CharacterId id{std::string{account_name},
                                          "Pre" + std::to_string(i)};
            domain::realm::CharacterStats stats;
            (void)list.add(domain::realm::Character{id, stats, core::SystemTime{}});
        }
        return core::Result<domain::realm::CharacterList, core::Error>(std::move(list));
    }

    core::Result<void, core::Error>
    save(std::string_view, const domain::realm::CharacterList&) override {
        if (fail_save)
            return core::fail(core::make_error(core::StatusCode::Internal, "list save boom"));
        return core::Result<void, core::Error>();
    }
};

}  // namespace pvpgn::application::realm

namespace pa = pvpgn::application::realm;

TEST_CASE("CreateCharacterUseCase: full account returns ResourceExhausted",
          "[application][realm][create_character]") {
    pa::FailingCharRepo       char_repo;
    pa::ConfigurableListRepo  list_repo;
    list_repo.preset = 8;  // default max capacity
    pa::CreateCharacterUseCase uc{char_repo, list_repo};

    pa::CreateCharacterCommand cmd;
    cmd.account_name = "alice";
    cmd.char_name    = "Overflow";

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::ResourceExhausted);
}

TEST_CASE("CreateCharacterUseCase: find_for_account error is propagated",
          "[application][realm][create_character]") {
    pa::FailingCharRepo       char_repo;
    pa::ConfigurableListRepo  list_repo;
    list_repo.fail_find = true;
    pa::CreateCharacterUseCase uc{char_repo, list_repo};

    pa::CreateCharacterCommand cmd;
    cmd.account_name = "alice";
    cmd.char_name    = "Bob";

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::Internal);
}

TEST_CASE("CreateCharacterUseCase: char_repo save failure is propagated",
          "[application][realm][create_character]") {
    pa::FailingCharRepo       char_repo;
    char_repo.fail_save = true;
    pa::ConfigurableListRepo  list_repo;
    pa::CreateCharacterUseCase uc{char_repo, list_repo};

    pa::CreateCharacterCommand cmd;
    cmd.account_name = "alice";
    cmd.char_name    = "Bob";

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::Internal);
}

TEST_CASE("CreateCharacterUseCase: list_repo save failure is propagated",
          "[application][realm][create_character]") {
    pa::FailingCharRepo       char_repo;
    pa::ConfigurableListRepo  list_repo;
    list_repo.fail_save = true;
    pa::CreateCharacterUseCase uc{char_repo, list_repo};

    pa::CreateCharacterCommand cmd;
    cmd.account_name = "alice";
    cmd.char_name    = "Bob";

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::Internal);
}

TEST_CASE("CreateCharacterUseCase: hardcore + expansion flags accepted",
          "[application][realm][create_character]") {
    pa::FailingCharRepo       char_repo;
    pa::ConfigurableListRepo  list_repo;
    pa::CreateCharacterUseCase uc{char_repo, list_repo};

    pa::CreateCharacterCommand cmd;
    cmd.account_name = "alice";
    cmd.char_name    = "Hero";
    cmd.char_class   = pvpgn::domain::realm::CharacterClass::barbarian;
    cmd.expansion    = true;
    cmd.hardcore     = true;

    auto result = uc.execute(cmd);
    REQUIRE(result.has_value());
}
