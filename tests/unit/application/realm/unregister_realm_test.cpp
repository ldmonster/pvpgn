// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for UnregisterRealm use case.

#include <catch2/catch_test_macros.hpp>

#include "application/realm/register_realm.hpp"
#include "application/realm/unregister_realm.hpp"
#include "domain/realm/ports.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/realm/realm.hpp"

#include <cctype>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace pvpgn::application::realm {

/// Reuse the same fake realm repository pattern from register_realm_test.cpp.
class FakeRealmRepo7 final : public pvpgn::domain::realm::IRealmRepository {
public:
    core::Result<domain::realm::Realm, core::Error>
    find_by_id(std::uint32_t id) const override {
        for (const auto& [key, r] : by_name_)
            if (r.id() == id)
                return core::Result<domain::realm::Realm, core::Error>(r);
        return core::fail(core::make_error(core::StatusCode::NotFound, "not found"));
    }

    core::Result<domain::realm::Realm, core::Error>
    find_by_name(const std::string& name) const override {
        auto it = by_name_.find(lower(name));
        if (it == by_name_.end())
            return core::fail(core::make_error(core::StatusCode::NotFound, "not found"));
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
        return core::fail(core::make_error(core::StatusCode::NotFound, "not found"));
    }

    void forEach(std::function<bool(const domain::realm::Realm&)> pred) const override {
        for (const auto& [key, r] : by_name_)
            if (!pred(r)) break;
    }

    std::size_t size() const noexcept override { return by_name_.size(); }

private:
    static std::string lower(std::string s) {
        for (auto& c : s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    std::unordered_map<std::string, domain::realm::Realm> by_name_;
};

}  // namespace pvpgn::application::realm

namespace pa = pvpgn::application::realm;

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("UnregisterRealm: happy path removes realm",
          "[application][realm][unregister_realm]") {
    pa::FakeRealmRepo7 repo;

    // Seed a realm via RegisterRealm (or directly via save)
    // We need a Realm with a known ID — use the repository directly.
    pa::RegisterRealmCommand reg_cmd;
    reg_cmd.name          = "USEast";
    reg_cmd.description   = "US East";
    reg_cmd.host          = "192.168.1.1";
    reg_cmd.port          = 6112;
    reg_cmd.password_hash = "abc";

    pa::RegisterRealm reg_uc{repo};
    auto reg_result = reg_uc.execute(reg_cmd);
    REQUIRE(reg_result.has_value());
    const auto realm_id = reg_result.value().realm_id;

    // Now unregister
    pa::UnregisterRealm uc{repo};
    auto result = uc.execute({realm_id});
    REQUIRE(result.has_value());
    CHECK(repo.size() == 0);
}

TEST_CASE("UnregisterRealm: non-existent realm_id returns NotFound",
          "[application][realm][unregister_realm]") {
    pa::FakeRealmRepo7 repo;
    pa::UnregisterRealm uc{repo};

    auto result = uc.execute({9999u});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::NotFound);
}

TEST_CASE("UnregisterRealm: removing one realm leaves others intact",
          "[application][realm][unregister_realm]") {
    pa::FakeRealmRepo7 repo;
    pa::RegisterRealm  reg_uc{repo};

    pa::RegisterRealmCommand cmd1;
    cmd1.name = "USEast"; cmd1.host = "1.1.1.1"; cmd1.port = 6112;
    auto r1 = reg_uc.execute(cmd1);
    REQUIRE(r1.has_value());

    pa::RegisterRealmCommand cmd2;
    cmd2.name = "USWest"; cmd2.host = "2.2.2.2"; cmd2.port = 6112;
    auto r2 = reg_uc.execute(cmd2);
    REQUIRE(r2.has_value());

    pa::UnregisterRealm uc{repo};
    REQUIRE(uc.execute({r1.value().realm_id}).has_value());

    CHECK(repo.size() == 1);
    CHECK(repo.find_by_name("USWest").has_value());
}
