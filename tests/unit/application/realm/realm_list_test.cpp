// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "application/realm/realm_list.hpp"

#include <string>
#include <utility>
#include <vector>

namespace pa = pvpgn::application::realm;

namespace {

class StaticRealmRepo : public pa::IRealmRepository {
public:
    explicit StaticRealmRepo(std::vector<pa::RealmListing> entries)
        : entries_(std::move(entries)) {}

    std::vector<pa::RealmListing> list_all() const override {
        return entries_;
    }

private:
    std::vector<pa::RealmListing> entries_;
};

pa::RealmListing make_realm(std::uint32_t id, std::string name,
                            std::string desc, bool active) {
    return pa::RealmListing{id, std::move(name), std::move(desc), active};
}

} // namespace

TEST_CASE("dispatch_realm_list returns empty when repo is empty", "[application][realm]") {
    StaticRealmRepo repo({});
    auto out = pa::dispatch_realm_list(repo);
    CHECK(out.active_entries.empty());
}

TEST_CASE("dispatch_realm_list returns empty when no realms are active", "[application][realm]") {
    StaticRealmRepo repo({
        make_realm(1, "r1", "desc1", false),
        make_realm(2, "r2", "desc2", false),
    });
    auto out = pa::dispatch_realm_list(repo);
    CHECK(out.active_entries.empty());
}

TEST_CASE("dispatch_realm_list returns only active realms in repo order", "[application][realm]") {
    StaticRealmRepo repo({
        make_realm(1, "r1", "desc1", true),
        make_realm(2, "r2", "desc2", false),
        make_realm(3, "r3", "desc3", true),
        make_realm(4, "r4", "desc4", true),
    });
    auto out = pa::dispatch_realm_list(repo);
    REQUIRE(out.active_entries.size() == 3);
    CHECK(out.active_entries[0].id == 1);
    CHECK(out.active_entries[0].name == "r1");
    CHECK(out.active_entries[0].description == "desc1");
    CHECK(out.active_entries[1].id == 3);
    CHECK(out.active_entries[1].name == "r3");
    CHECK(out.active_entries[2].id == 4);
    CHECK(out.active_entries[2].name == "r4");
}

TEST_CASE("dispatch_realm_list passes through all metadata fields", "[application][realm]") {
    StaticRealmRepo repo({
        make_realm(42, "diablo2", "Realm of darkness", true),
    });
    auto out = pa::dispatch_realm_list(repo);
    REQUIRE(out.active_entries.size() == 1);
    const auto& e = out.active_entries[0];
    CHECK(e.id == 42u);
    CHECK(e.name == "diablo2");
    CHECK(e.description == "Realm of darkness");
    CHECK(e.active == true);
}

TEST_CASE("dispatch_realm_list keeps single active realm", "[application][realm]") {
    StaticRealmRepo repo({
        make_realm(1, "only", "only realm", true),
    });
    auto out = pa::dispatch_realm_list(repo);
    REQUIRE(out.active_entries.size() == 1);
    CHECK(out.active_entries[0].name == "only");
}

TEST_CASE("dispatch_realm_list filters single-inactive correctly", "[application][realm]") {
    StaticRealmRepo repo({
        make_realm(1, "down", "offline", false),
    });
    auto out = pa::dispatch_realm_list(repo);
    CHECK(out.active_entries.empty());
}
