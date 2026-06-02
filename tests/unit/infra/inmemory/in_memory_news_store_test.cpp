// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "infra/inmemory/in_memory_news_store.hpp"
#include "domain/social/ports/news_store.hpp"

namespace pvpgn::infra::inmemory {

TEST_CASE("InMemoryNewsStore: GetNewsEmptyStore", "[infra][inmemory]") {
    InMemoryNewsStore store;
    auto result = store.get_news(10);
    REQUIRE(result.has_value());
    REQUIRE(result.value().empty());
}

TEST_CASE("InMemoryNewsStore: AddAndGetSingleItem", "[infra][inmemory]") {
    InMemoryNewsStore store;

    application::ports::NewsItem item;
    item.text = "Server maintenance tonight";

    REQUIRE(store.add_news(item).has_value());

    auto result = store.get_news(10);
    REQUIRE(result.has_value());
    REQUIRE(result.value().size() == 1);
    REQUIRE(result.value()[0].text == "Server maintenance tonight");
}

TEST_CASE("InMemoryNewsStore: GetNewsReturnsNewestFirst", "[infra][inmemory]") {
    InMemoryNewsStore store;

    application::ports::NewsItem item1;
    item1.text = "oldest";
    REQUIRE(store.add_news(item1).has_value());

    application::ports::NewsItem item2;
    item2.text = "middle";
    REQUIRE(store.add_news(item2).has_value());

    application::ports::NewsItem item3;
    item3.text = "newest";
    REQUIRE(store.add_news(item3).has_value());

    auto result = store.get_news(10);
    REQUIRE(result.has_value());
    REQUIRE(result.value().size() == 3);
    // Newest first
    REQUIRE(result.value()[0].text == "newest");
    REQUIRE(result.value()[1].text == "middle");
    REQUIRE(result.value()[2].text == "oldest");
}

TEST_CASE("InMemoryNewsStore: GetNewsRespectsMaxItems", "[infra][inmemory]") {
    InMemoryNewsStore store;

    for (int i = 0; i < 5; ++i) {
        application::ports::NewsItem item;
        item.text = "item" + std::to_string(i);
        REQUIRE(store.add_news(item).has_value());
    }

    auto result = store.get_news(3);
    REQUIRE(result.has_value());
    REQUIRE(result.value().size() == 3);
}

TEST_CASE("InMemoryNewsStore: GetNewsMaxItemsLargerThanStore", "[infra][inmemory]") {
    InMemoryNewsStore store;

    application::ports::NewsItem item;
    item.text = "only item";
    REQUIRE(store.add_news(item).has_value());

    auto result = store.get_news(100);
    REQUIRE(result.has_value());
    REQUIRE(result.value().size() == 1);
}

TEST_CASE("InMemoryNewsStore: GetNewsZeroMaxItems", "[infra][inmemory]") {
    InMemoryNewsStore store;

    application::ports::NewsItem item;
    item.text = "some news";
    REQUIRE(store.add_news(item).has_value());

    auto result = store.get_news(0);
    REQUIRE(result.has_value());
    REQUIRE(result.value().empty());
}

}  // namespace pvpgn::infra::inmemory
