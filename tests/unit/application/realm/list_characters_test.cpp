// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for ListCharactersUseCase.

#include <catch2/catch_test_macros.hpp>

#include "application/realm/list_characters.hpp"
#include "core/result.hpp"
#include "domain/realm/character.hpp"
#include "domain/realm/character_list.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pvpgn::application::realm {

class FakeListRepo3 final : public ICharacterListRepository {
public:
    core::Result<domain::realm::CharacterList, core::Error>
    find_for_account(std::string_view account_name) override {
        domain::realm::CharacterList list{std::string{account_name}};
        auto it = lists_.find(std::string{account_name});
        if (it != lists_.end())
            for (auto& c : it->second)
                (void)list.add(c);
        return core::Result<domain::realm::CharacterList, core::Error>(std::move(list));
    }

    core::Result<void, core::Error>
    save(std::string_view account_name,
         const domain::realm::CharacterList& list) override {
        auto& stored = lists_[std::string{account_name}];
        stored.clear();
        for (auto* cp : list.list(domain::realm::SortMode::by_name))
            stored.push_back(*cp);
        return core::Result<void, core::Error>();
    }

    void seed(std::string_view account_name, domain::realm::Character c) {
        lists_[std::string{account_name}].push_back(std::move(c));
    }

private:
    std::unordered_map<std::string,
                       std::vector<domain::realm::Character>> lists_;
};

}  // namespace pvpgn::application::realm

namespace pa = pvpgn::application::realm;
namespace dr = pvpgn::domain::realm;

static dr::Character make_char3(std::string account, std::string name,
                                 dr::CharacterClass cls = dr::CharacterClass::amazon) {
    dr::CharacterId id{std::move(account), std::move(name)};
    dr::CharacterStats stats;
    stats.char_class = cls;
    return dr::Character{std::move(id), stats, {}};
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("ListCharactersUseCase: empty account returns empty list",
          "[application][realm][list_characters]") {
    pa::FakeListRepo3 list_repo;
    pa::ListCharactersUseCase uc{list_repo};

    pa::ListCharactersQuery q{"alice"};
    auto result = uc.execute(q);
    REQUIRE(result.has_value());
    CHECK(result.value().characters.empty());
}

TEST_CASE("ListCharactersUseCase: returns all characters sorted by name",
          "[application][realm][list_characters]") {
    pa::FakeListRepo3 list_repo;
    list_repo.seed("alice", make_char3("alice", "Zorro"));
    list_repo.seed("alice", make_char3("alice", "Amazon"));
    list_repo.seed("alice", make_char3("alice", "Mage"));

    pa::ListCharactersUseCase uc{list_repo};
    pa::ListCharactersQuery q{"alice", dr::SortMode::by_name};
    auto result = uc.execute(q);
    REQUIRE(result.has_value());
    REQUIRE(result.value().characters.size() == 3);
    CHECK(result.value().characters[0].char_name == "Amazon");
    CHECK(result.value().characters[1].char_name == "Mage");
    CHECK(result.value().characters[2].char_name == "Zorro");
}

TEST_CASE("ListCharactersUseCase: empty account_name returns InvalidArgument",
          "[application][realm][list_characters]") {
    pa::FakeListRepo3 list_repo;
    pa::ListCharactersUseCase uc{list_repo};

    auto result = uc.execute({""});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::InvalidArgument);
}

TEST_CASE("ListCharactersUseCase: character class is preserved in summary",
          "[application][realm][list_characters]") {
    pa::FakeListRepo3 list_repo;
    list_repo.seed("bob", make_char3("bob", "Necro", dr::CharacterClass::necromancer));

    pa::ListCharactersUseCase uc{list_repo};
    auto result = uc.execute({"bob"});
    REQUIRE(result.has_value());
    REQUIRE(result.value().characters.size() == 1);
    CHECK(result.value().characters[0].char_class == dr::CharacterClass::necromancer);
}
