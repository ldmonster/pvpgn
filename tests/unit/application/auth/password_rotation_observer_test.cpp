// SPDX-License-Identifier: GPL-2.0-or-later
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "application/auth/password_rotation_observer.hpp"
#include "core/logging.hpp"
#include "domain/shared/events.hpp"
#include "domain/shared/ids.hpp"
#include "infra/inmemory/event_bus.hpp"

using namespace pvpgn;

namespace {

struct RecordingLogger final : core::ILogger {
    struct Rec {
        core::LogLevel level;
        std::string    module;
        std::string    message;
    };
    std::vector<Rec> records;

    void log(core::LogLevel l, std::string_view m,
             std::string_view msg) noexcept override {
        try {
            records.push_back(Rec{l, std::string{m}, std::string{msg}});
        } catch (...) {
        }
    }
    core::LogLevel level() const noexcept override {
        return core::LogLevel::Trace;
    }
    void set_level(core::LogLevel) noexcept override {}
};

}  // namespace

TEST_CASE("PasswordRotationObserver logs required events",
          "[application][auth][rotation]") {
    RecordingLogger     logger;
    infra::inmemory::InMemoryEventBus bus;
    application::auth::PasswordRotationObserver obs(logger, bus);

    bus.publish(domain::events::AccountPasswordRotationRequired{
        domain::AccountId{42}});

    REQUIRE(logger.records.size() == 1);
    REQUIRE(logger.records[0].level == core::LogLevel::Info);
    REQUIRE(logger.records[0].module
            == application::auth::PasswordRotationObserver::kChannel);
    REQUIRE_THAT(logger.records[0].message,
                 Catch::Matchers::ContainsSubstring("set"));
    REQUIRE_THAT(logger.records[0].message,
                 Catch::Matchers::ContainsSubstring("42"));
}

TEST_CASE("PasswordRotationObserver logs cleared events",
          "[application][auth][rotation]") {
    RecordingLogger     logger;
    infra::inmemory::InMemoryEventBus bus;
    application::auth::PasswordRotationObserver obs(logger, bus);

    bus.publish(domain::events::AccountPasswordRotationCleared{
        domain::AccountId{7}});

    REQUIRE(logger.records.size() == 1);
    REQUIRE_THAT(logger.records[0].message,
                 Catch::Matchers::ContainsSubstring("cleared"));
    REQUIRE_THAT(logger.records[0].message,
                 Catch::Matchers::ContainsSubstring("7"));
}

TEST_CASE("PasswordRotationObserver ignores unrelated events",
          "[application][auth][rotation]") {
    RecordingLogger     logger;
    infra::inmemory::InMemoryEventBus bus;
    application::auth::PasswordRotationObserver obs(logger, bus);

    bus.publish(domain::events::AccountPasswordChanged{
        domain::AccountId{1}});

    REQUIRE(logger.records.empty());
}

TEST_CASE("PasswordRotationObserver unsubscribes on destruction",
          "[application][auth][rotation]") {
    RecordingLogger     logger;
    infra::inmemory::InMemoryEventBus bus;
    {
        application::auth::PasswordRotationObserver obs(logger, bus);
        bus.publish(domain::events::AccountPasswordRotationRequired{
            domain::AccountId{1}});
    }
    REQUIRE(logger.records.size() == 1);
    bus.publish(domain::events::AccountPasswordRotationRequired{
        domain::AccountId{2}});
    // Post-dtor publish must not reach the observer.
    REQUIRE(logger.records.size() == 1);
}
