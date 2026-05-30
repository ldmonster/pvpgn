// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "application/realm/realm_auth.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/realm/realm.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace pvpgn::application::realm {

/// Inline fake realm repository for auth tests.
class FakeAuthRealmRepository final : public ports::IRealmRepository {
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
        auto it = by_name_.find(name);
        if (it == by_name_.end()) {
            return core::fail(core::make_error(core::StatusCode::NotFound,
                                               "realm not found"));
        }
        return core::Result<domain::realm::Realm, core::Error>(it->second);
    }

    core::Result<void, core::Error>
    save(const domain::realm::Realm& realm) override {
        by_name_.insert_or_assign(realm.name(), realm);
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

    /// Helper: seed the repository with a realm directly.
    void seed(domain::realm::Realm realm) {
        by_name_[realm.name()] = std::move(realm);
    }

private:
    std::unordered_map<std::string, domain::realm::Realm> by_name_;
};

/// Inline fake credential store for auth tests.
class FakeRealmCredentialStore final : public IRealmCredentialStore {
public:
    core::Result<std::string, core::Error>
    find_password_hash(const std::string& realm_name) const override {
        auto it = hashes_.find(realm_name);
        if (it == hashes_.end()) {
            return core::fail(core::make_error(core::StatusCode::NotFound,
                                               "no credential for realm"));
        }
        return core::Result<std::string, core::Error>(it->second);
    }

    void set(const std::string& realm_name, std::string hash) {
        hashes_[realm_name] = std::move(hash);
    }

private:
    std::unordered_map<std::string, std::string> hashes_;
};

} // namespace pvpgn::application::realm

namespace pa = pvpgn::application::realm;

TEST_CASE("RealmAuth happy path: correct password returns realm ID and name",
          "[application][realm][realm_auth]") {
    pa::FakeAuthRealmRepository repo;
    pa::FakeRealmCredentialStore creds;

    // Seed a realm and its credential
    auto create_result = pvpgn::domain::realm::Realm::create(1, "USEast", "US East");
    REQUIRE(create_result.has_value());
    repo.seed(create_result.value());
    creds.set("USEast", "correct_hash");

    pa::RealmAuth use_case{repo, creds};

    pa::RealmAuthCommand cmd;
    cmd.realm_name    = "USEast";
    cmd.password_hash = "correct_hash";

    auto result = use_case.execute(cmd);

    REQUIRE(result.has_value());
    CHECK(result.value().realm_id == 1);
    CHECK(result.value().realm_name == "USEast");
}

TEST_CASE("RealmAuth unknown realm name returns NotFound",
          "[application][realm][realm_auth]") {
    pa::FakeAuthRealmRepository repo;   // empty
    pa::FakeRealmCredentialStore creds; // empty

    pa::RealmAuth use_case{repo, creds};

    pa::RealmAuthCommand cmd;
    cmd.realm_name    = "NonExistent";
    cmd.password_hash = "any_hash";

    auto result = use_case.execute(cmd);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("RealmAuth wrong password returns PermissionDenied",
          "[application][realm][realm_auth]") {
    pa::FakeAuthRealmRepository repo;
    pa::FakeRealmCredentialStore creds;

    // Seed a realm and its credential
    auto create_result = pvpgn::domain::realm::Realm::create(2, "Europe", "European realm");
    REQUIRE(create_result.has_value());
    repo.seed(create_result.value());
    creds.set("Europe", "stored_hash");

    pa::RealmAuth use_case{repo, creds};

    pa::RealmAuthCommand cmd;
    cmd.realm_name    = "Europe";
    cmd.password_hash = "wrong_hash";  // does not match

    auto result = use_case.execute(cmd);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == core::StatusCode::PermissionDenied);
}
