// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "application/realm/heartbeat_realm.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/realm/realm.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

#include "domain/realm/ports.hpp"

namespace pvpgn::application::realm {

/// Inline fake: stores realms in a map keyed by id.
/// Tracks how many times save() was called so tests can assert the
/// heartbeat actually persisted the realm.
class FakeHeartbeatRealmRepository final : public pvpgn::domain::realm::IRealmRepository {
public:
    int save_call_count = 0;

    core::Result<domain::realm::Realm, core::Error>
    find_by_id(std::uint32_t id) const override {
        auto it = by_id_.find(id);
        if (it == by_id_.end()) {
            return core::fail(core::make_error(core::StatusCode::NotFound,
                                               "realm not found"));
        }
        return core::Result<domain::realm::Realm, core::Error>(it->second);
    }

    core::Result<domain::realm::Realm, core::Error>
    find_by_name(const std::string& name) const override {
        for (const auto& [id, r] : by_id_) {
            if (r.name() == name) {
                return core::Result<domain::realm::Realm, core::Error>(r);
            }
        }
        return core::fail(core::make_error(core::StatusCode::NotFound,
                                           "realm not found"));
    }

    core::Result<void, core::Error>
    save(const domain::realm::Realm& realm) override {
        ++save_call_count;
        by_id_.insert_or_assign(realm.id(), realm);
        return core::Result<void, core::Error>();
    }

    core::Result<void, core::Error>
    remove(std::uint32_t id) override {
        auto it = by_id_.find(id);
        if (it == by_id_.end()) {
            return core::fail(core::make_error(core::StatusCode::NotFound,
                                               "realm not found"));
        }
        by_id_.erase(it);
        return core::Result<void, core::Error>();
    }

    void forEach(std::function<bool(const domain::realm::Realm&)> pred) const override {
        for (const auto& [id, r] : by_id_) {
            if (!pred(r)) break;
        }
    }

    std::size_t size() const noexcept override { return by_id_.size(); }

    /// Helper: seed the repository with a realm directly.
    void seed(domain::realm::Realm realm) {
        by_id_.insert_or_assign(realm.id(), std::move(realm));
    }

private:
    std::unordered_map<std::uint32_t, domain::realm::Realm> by_id_;
};

} // namespace pvpgn::application::realm

namespace pa = pvpgn::application::realm;

TEST_CASE("HeartbeatRealm happy path: existing realm is re-saved",
          "[application][realm][heartbeat_realm]") {
    pa::FakeHeartbeatRealmRepository repo;

    // Seed a realm
    auto create_result = pvpgn::domain::realm::Realm::create(1, "USEast", "US East");
    REQUIRE(create_result.has_value());
    repo.seed(create_result.value());

    pa::HeartbeatRealm use_case{repo};

    pa::HeartbeatRealmCommand cmd;
    cmd.realm_id = 1;
    cmd.now      = std::chrono::system_clock::now();

    const int saves_before = repo.save_call_count;
    auto result = use_case.execute(cmd);

    REQUIRE(result.has_value());
    // The use case must have called save() exactly once
    CHECK(repo.save_call_count == saves_before + 1);
}

TEST_CASE("HeartbeatRealm unknown realm returns NotFound",
          "[application][realm][heartbeat_realm]") {
    pa::FakeHeartbeatRealmRepository repo;  // empty — no realms seeded

    pa::HeartbeatRealm use_case{repo};

    pa::HeartbeatRealmCommand cmd;
    cmd.realm_id = 99;
    cmd.now      = std::chrono::system_clock::now();

    auto result = use_case.execute(cmd);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::NotFound);
}
