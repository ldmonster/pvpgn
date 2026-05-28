// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>

#include <catch2/catch_test_macros.hpp>

#include "infra/inmemory/in_memory_channel_store.hpp"

namespace pvpgn::infra::inmemory {

TEST_CASE("InMemoryChannelStore: LoadAllEmptyStore", "[infra][inmemory]") {
    InMemoryChannelStore store;
    auto result = store.load_all();
    REQUIRE(result.has_value());
    REQUIRE(result.value().empty());
}

TEST_CASE("InMemoryChannelStore: SaveAndLoadAll", "[infra][inmemory]") {
    InMemoryChannelStore store;

    application::ports::ChannelDefinition def;
    def.name         = "Lobby";
    def.topic        = "Welcome";
    def.max_users    = 100;
    def.is_permanent = true;
    def.is_moderated = false;

    REQUIRE(store.save(def).has_value());

    auto result = store.load_all();
    REQUIRE(result.has_value());
    REQUIRE(result.value().size() == 1);
    REQUIRE(result.value()[0].name  == "Lobby");
    REQUIRE(result.value()[0].topic == "Welcome");
    REQUIRE(result.value()[0].max_users == 100);
    REQUIRE(result.value()[0].is_permanent == true);
}

TEST_CASE("InMemoryChannelStore: SaveMultipleChannels", "[infra][inmemory]") {
    InMemoryChannelStore store;

    for (int i = 0; i < 3; ++i) {
        application::ports::ChannelDefinition def;
        def.name = "Channel" + std::to_string(i);
        REQUIRE(store.save(def).has_value());
    }

    auto result = store.load_all();
    REQUIRE(result.has_value());
    REQUIRE(result.value().size() == 3);
}

TEST_CASE("InMemoryChannelStore: SaveOverwritesExistingByName", "[infra][inmemory]") {
    InMemoryChannelStore store;

    application::ports::ChannelDefinition def1;
    def1.name  = "General";
    def1.topic = "old topic";
    REQUIRE(store.save(def1).has_value());

    application::ports::ChannelDefinition def2;
    def2.name  = "General";
    def2.topic = "new topic";
    REQUIRE(store.save(def2).has_value());

    auto result = store.load_all();
    REQUIRE(result.has_value());
    REQUIRE(result.value().size() == 1);
    REQUIRE(result.value()[0].topic == "new topic");
}

TEST_CASE("InMemoryChannelStore: RemoveExistingChannel", "[infra][inmemory]") {
    InMemoryChannelStore store;

    application::ports::ChannelDefinition def;
    def.name = "ToRemove";
    REQUIRE(store.save(def).has_value());

    auto remove_result = store.remove("ToRemove");
    REQUIRE(remove_result.has_value());

    auto result = store.load_all();
    REQUIRE(result.has_value());
    REQUIRE(result.value().empty());
}

TEST_CASE("InMemoryChannelStore: RemoveNotFoundReturnsError", "[infra][inmemory]") {
    InMemoryChannelStore store;
    auto result = store.remove("NonExistent");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("InMemoryChannelStore: RemoveDoesNotAffectOtherChannels", "[infra][inmemory]") {
    InMemoryChannelStore store;

    application::ports::ChannelDefinition d1;
    d1.name = "Keep";
    REQUIRE(store.save(d1).has_value());

    application::ports::ChannelDefinition d2;
    d2.name = "Delete";
    REQUIRE(store.save(d2).has_value());

    REQUIRE(store.remove("Delete").has_value());

    auto result = store.load_all();
    REQUIRE(result.has_value());
    REQUIRE(result.value().size() == 1);
    REQUIRE(result.value()[0].name == "Keep");
}

TEST_CASE("InMemoryChannelStore: SavePreservesAllFields", "[infra][inmemory]") {
    InMemoryChannelStore store;

    application::ports::ChannelDefinition def;
    def.name         = "Moderated";
    def.topic        = "Strict rules";
    def.max_users    = 50;
    def.is_permanent = false;
    def.is_moderated = true;

    REQUIRE(store.save(def).has_value());

    auto result = store.load_all();
    REQUIRE(result.has_value());
    REQUIRE(result.value().size() == 1);

    const auto& loaded = result.value()[0];
    REQUIRE(loaded.name         == "Moderated");
    REQUIRE(loaded.topic        == "Strict rules");
    REQUIRE(loaded.max_users    == 50u);
    REQUIRE(loaded.is_permanent == false);
    REQUIRE(loaded.is_moderated == true);
}

}  // namespace pvpgn::infra::inmemory
