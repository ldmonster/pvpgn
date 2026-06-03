// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "application/realm/register_realm.hpp"
#include "core/result.hpp"
#include "domain/realm/realm.hpp"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

#include "domain/realm/ports.hpp"

namespace pvpgn::application::realm {

/// Inline fake: stores realms in a map keyed by lower-case name.
class FakeRealmRepository final : public pvpgn::domain::realm::IRealmRepository {
public:
    core::Result<domain::realm::Realm, core::Error>
    find_by_id(std::uint32_t id) const override {
        for (const auto& [key, r] : by_name_) {
            if (r.id() == id) {
                return core::Result<domain::realm::Realm, core::Error>(r);
            }
        }
        return core::fail(core::make_error(core::StatusCode::NotFound,
                                           "realm not found"));
    }

    core::Result<domain::realm::Realm, core::Error>
    find_by_name(const std::string& name) const override {
        auto it = by_name_.find(lower(name));
        if (it == by_name_.end()) {
            return core::fail(core::make_error(core::StatusCode::NotFound,
                                               "realm not found"));
        }
        return core::Result<domain::realm::Realm, core::Error>(it->second);
    }

    core::Result<void, core::Error>
    save(const domain::realm::Realm& realm) override {
        by_name_.insert_or_assign(lower(realm.name()), realm);
        return core::Result<void, core::Error>();
    }

    core::Result<void, core::Error>
    remove(std::uint32_t id) override {
        for (auto it = by_name_.begin(); it != by_name_.end(); ++it) {
            if (it->second.id() == id) {
                by_name_.erase(it);
                return core::Result<void, core::Error>();
            }
        }
        return core::fail(core::make_error(core::StatusCode::NotFound,
                                           "realm not found"));
    }

    void forEach(std::function<bool(const domain::realm::Realm&)> pred) const override {
        for (const auto& [key, r] : by_name_) {
            if (!pred(r)) break;
        }
    }

    std::size_t size() const noexcept override { return by_name_.size(); }

private:
    static std::string lower(std::string s) {
        for (auto& c : s) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return s;
    }

    std::unordered_map<std::string, domain::realm::Realm> by_name_;
};

} // namespace pvpgn::application::realm

namespace pa   = pvpgn::application::realm;
namespace core = pvpgn::core;  // the TEST_CASEs below are at global scope

TEST_CASE("RegisterRealm happy path: realm is registered and ID returned",
          "[application][realm][register_realm]") {
    pa::FakeRealmRepository repo;
    pa::RegisterRealm use_case{repo};

    pa::RegisterRealmCommand cmd;
    cmd.name          = "USEast";
    cmd.description   = "US East realm";
    cmd.host          = "192.168.1.1";
    cmd.port          = 6112;
    cmd.password_hash = "abc123";

    auto result = use_case.execute(cmd);

    REQUIRE(result.has_value());
    CHECK(result.value().realm_id != 0);

    // The realm should now be findable by name
    auto found = repo.find_by_name("USEast");
    REQUIRE(found.has_value());
    CHECK(found.value().name() == "USEast");
    CHECK(found.value().description() == "US East realm");
}

TEST_CASE("RegisterRealm duplicate name returns Conflict",
          "[application][realm][register_realm]") {
    pa::FakeRealmRepository repo;
    pa::RegisterRealm use_case{repo};

    pa::RegisterRealmCommand cmd;
    cmd.name          = "USEast";
    cmd.description   = "US East realm";
    cmd.host          = "192.168.1.1";
    cmd.port          = 6112;
    cmd.password_hash = "abc123";

    // First registration succeeds
    auto first = use_case.execute(cmd);
    REQUIRE(first.has_value());

    // Second registration with the same name must fail
    auto second = use_case.execute(cmd);
    REQUIRE_FALSE(second.has_value());
    CHECK(second.error().code() == core::StatusCode::Conflict);
}

TEST_CASE("RegisterRealm empty name returns InvalidArgument",
          "[application][realm][register_realm]") {
    pa::FakeRealmRepository repo;
    pa::RegisterRealm use_case{repo};

    pa::RegisterRealmCommand cmd;
    cmd.name          = "";   // invalid
    cmd.description   = "desc";
    cmd.host          = "192.168.1.1";
    cmd.port          = 6112;
    cmd.password_hash = "abc123";

    auto result = use_case.execute(cmd);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("RegisterRealm empty host returns InvalidArgument",
          "[application][realm][register_realm]") {
    pa::FakeRealmRepository repo;
    pa::RegisterRealm use_case{repo};

    pa::RegisterRealmCommand cmd;
    cmd.name          = "USEast";
    cmd.description   = "desc";
    cmd.host          = "";   // invalid
    cmd.port          = 6112;
    cmd.password_hash = "abc123";

    auto result = use_case.execute(cmd);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == core::StatusCode::InvalidArgument);
}
