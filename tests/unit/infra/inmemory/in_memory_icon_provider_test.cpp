// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstddef>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "infra/inmemory/in_memory_icon_provider.hpp"

namespace pvpgn::infra::inmemory {

TEST_CASE("InMemoryIconProvider: IconForUnknownTagReturnsNullopt", "[infra][inmemory]") {
    InMemoryIconProvider provider;
    auto result = provider.icon_for("STAR");
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("InMemoryIconProvider: SetIconAndLookup", "[infra][inmemory]") {
    InMemoryIconProvider provider;
    provider.set_icon("STAR", "STAR");

    auto result = provider.icon_for("STAR");
    REQUIRE(result.has_value());
    REQUIRE(result.value() == "STAR");
}

TEST_CASE("InMemoryIconProvider: OverwriteExistingIcon", "[infra][inmemory]") {
    InMemoryIconProvider provider;
    provider.set_icon("W2BN", "W2BN");
    provider.set_icon("W2BN", "ICON");

    auto result = provider.icon_for("W2BN");
    REQUIRE(result.has_value());
    REQUIRE(result.value() == "ICON");
}

TEST_CASE("InMemoryIconProvider: MultipleIconsDoNotInterfere", "[infra][inmemory]") {
    InMemoryIconProvider provider;
    provider.set_icon("STAR", "STAR");
    provider.set_icon("SEXP", "SEXP");
    provider.set_icon("W3XP", "W3XP");

    REQUIRE(provider.icon_for("STAR").value() == "STAR");
    REQUIRE(provider.icon_for("SEXP").value() == "SEXP");
    REQUIRE(provider.icon_for("W3XP").value() == "W3XP");
    REQUIRE_FALSE(provider.icon_for("D2DV").has_value());
}

TEST_CASE("InMemoryIconProvider: RawIconDataEmptyByDefault", "[infra][inmemory]") {
    InMemoryIconProvider provider;
    auto data = provider.raw_icon_data();
    REQUIRE(data.empty());
}

TEST_CASE("InMemoryIconProvider: SetRawDataAndRetrieve", "[infra][inmemory]") {
    InMemoryIconProvider provider;

    std::vector<std::byte> raw = {
        std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04}
    };
    provider.set_raw_data(raw);

    auto data = provider.raw_icon_data();
    REQUIRE(data.size() == 4);
    REQUIRE(data[0] == std::byte{0x01});
    REQUIRE(data[3] == std::byte{0x04});
}

TEST_CASE("InMemoryIconProvider: OverwriteRawData", "[infra][inmemory]") {
    InMemoryIconProvider provider;

    provider.set_raw_data({std::byte{0xAA}, std::byte{0xBB}});
    provider.set_raw_data({std::byte{0xFF}});

    auto data = provider.raw_icon_data();
    REQUIRE(data.size() == 1);
    REQUIRE(data[0] == std::byte{0xFF});
}

}  // namespace pvpgn::infra::inmemory
