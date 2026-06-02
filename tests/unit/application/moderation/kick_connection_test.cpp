// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::moderation::KickConnection`. Exercises the use-case
// for immediately disconnecting a session.

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <span>
#include <vector>

#include "application/moderation/kick_connection.hpp"
#include "domain/connection/ports.hpp"
#include "domain/identity/ports.hpp"
#include "domain/shared/ids.hpp"

namespace {

using namespace pvpgn;
using application::moderation::KickConnection;
using application::moderation::KickConnectionError;

// ---------------------------------------------------------------------------
// Fakes
// ---------------------------------------------------------------------------

class FakeSessionRegistry final : public domain::identity::ISessionRegistry {
public:
    bool session_exists = true;
    bool detach_called  = false;

    core::Status<>
    attach(domain::SessionId, domain::AccountId) override { return core::ok(); }

    void detach(domain::SessionId) override { detach_called = true; }

    std::optional<domain::SessionId>
    session_for(domain::AccountId) const override { return std::nullopt; }

    std::optional<domain::AccountId>
    account_for(domain::SessionId) const override {
        if (!session_exists) return std::nullopt;
        return domain::AccountId{42};
    }

    std::vector<domain::SessionId> list() const override { return {}; }
};

class FakeMessageRouter final : public domain::connection::IMessageRouter {
public:
    core::Result<void, core::Error>
    send(domain::SessionId, std::span<const std::byte>) override { return {}; }

    core::Result<void, core::Error>
    broadcast(std::span<const domain::SessionId>,
              std::span<const std::byte>) override { return {}; }

    core::Result<void, core::Error>
    send_to_account(domain::AccountId,
                    std::span<const std::byte>) override { return {}; }
};

struct Fixture {
    std::shared_ptr<FakeSessionRegistry> registry = std::make_shared<FakeSessionRegistry>();
    std::shared_ptr<FakeMessageRouter>   router   = std::make_shared<FakeMessageRouter>();

    KickConnection make_use_case() {
        return KickConnection{registry, router};
    }
};

}  // namespace

TEST_CASE("KickConnection: happy path disconnects an active session",
          "[application][moderation][kick_connection]") {
    Fixture f;
    auto uc = f.make_use_case();

    auto result = uc.execute(domain::SessionId{100}, "admin kick");
    REQUIRE(result);
    REQUIRE(f.registry->detach_called);
}

TEST_CASE("KickConnection: returns SessionNotFound when session does not exist",
          "[application][moderation][kick_connection]") {
    Fixture f;
    f.registry->session_exists = false;
    auto uc = f.make_use_case();

    auto result = uc.execute(domain::SessionId{999}, "admin kick");
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == KickConnectionError::SessionNotFound);
}

TEST_CASE("KickConnection: detach is not called when session not found",
          "[application][moderation][kick_connection]") {
    Fixture f;
    f.registry->session_exists = false;
    auto uc = f.make_use_case();

    (void)uc.execute(domain::SessionId{999}, "admin kick");
    REQUIRE_FALSE(f.registry->detach_called);
}
