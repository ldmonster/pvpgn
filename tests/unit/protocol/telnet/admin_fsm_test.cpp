// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "domain/chat/ports/command_registry.hpp"
#include "domain/moderation/ports.hpp"
#include "core/bytes.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "protocol/telnet/admin_fsm.hpp"
#include "protocol/telnet/telnet_session_context.hpp"

namespace pt = pvpgn::protocol::telnet;
namespace ap = pvpgn::application::ports;

namespace {

class FakeCtx : public pt::ITelnetSessionContext {
public:
    pvpgn::core::Status<> send(pvpgn::core::ByteView b) override {
        sent.append(reinterpret_cast<const char*>(b.data()), b.size());
        return pvpgn::core::ok();
    }
    pvpgn::core::Status<> send_line(std::string_view t) override {
        sent.append(t);
        sent.append("\r\n");
        return pvpgn::core::ok();
    }
    void close() override { closed = true; }

    std::string sent;
    bool        closed = false;
};

class FakePerms : public ap::IPermissionChecker {
public:
    bool has_permission(pvpgn::domain::AccountId, ap::Permission) const override {
        return true;
    }
    bool has_command_group(pvpgn::domain::AccountId, std::string_view) const override {
        return true;
    }
};

class FakeRegistry : public ap::ICommandRegistry {
public:
    pvpgn::core::Result<std::string, pvpgn::core::Error>
    dispatch(pvpgn::domain::AccountId,
             std::string_view line,
             const ap::IPermissionChecker&) const override {
        ++calls;
        last_line.assign(line);
        if (line == "fail") {
            return pvpgn::core::fail(pvpgn::core::make_error(
                pvpgn::core::StatusCode::NotFound, "no such command"));
        }
        if (line == "silent") {
            return std::string{};
        }
        return std::string{"ok:"} + std::string(line);
    }
    std::vector<std::string>
    list_available(pvpgn::domain::AccountId,
                   const ap::IPermissionChecker&) const override {
        return {};
    }

    mutable int         calls = 0;
    mutable std::string last_line;
};

}  // namespace

TEST_CASE("on_connected sends banner + prompt", "[protocol][telnet][admin]") {
    auto ctx   = std::make_shared<FakeCtx>();
    auto perms = std::make_shared<FakePerms>();
    auto reg   = std::make_shared<FakeRegistry>();
    pt::TelnetAdminFsm fsm(ctx, reg, perms);

    REQUIRE(fsm.on_connected());

    CHECK(ctx->sent == "=== pvpgn admin console ===\r\n> ");
}

TEST_CASE("on_line dispatches and echoes reply + prompt",
          "[protocol][telnet][admin]") {
    auto ctx   = std::make_shared<FakeCtx>();
    auto perms = std::make_shared<FakePerms>();
    auto reg   = std::make_shared<FakeRegistry>();
    pt::TelnetAdminFsm fsm(ctx, reg, perms);

    REQUIRE(fsm.on_line("hello\r\n"));
    CHECK(reg->calls == 1);
    CHECK(reg->last_line == "hello");
    CHECK(ctx->sent == "ok:hello\r\n> ");
}

TEST_CASE("on_line maps registry error to 'error: <code>' line",
          "[protocol][telnet][admin]") {
    auto ctx   = std::make_shared<FakeCtx>();
    auto perms = std::make_shared<FakePerms>();
    auto reg   = std::make_shared<FakeRegistry>();
    pt::TelnetAdminFsm fsm(ctx, reg, perms);

    REQUIRE(fsm.on_line("fail"));
    CHECK(ctx->sent == "error: NotFound\r\n> ");
}

TEST_CASE("on_line skips empty reply lines but still re-prompts",
          "[protocol][telnet][admin]") {
    auto ctx   = std::make_shared<FakeCtx>();
    auto perms = std::make_shared<FakePerms>();
    auto reg   = std::make_shared<FakeRegistry>();
    pt::TelnetAdminFsm fsm(ctx, reg, perms);

    REQUIRE(fsm.on_line("silent"));
    CHECK(ctx->sent == "> ");
}

TEST_CASE("quit/exit close the session and skip dispatch",
          "[protocol][telnet][admin]") {
    auto ctx   = std::make_shared<FakeCtx>();
    auto perms = std::make_shared<FakePerms>();
    auto reg   = std::make_shared<FakeRegistry>();
    pt::TelnetAdminFsm fsm(ctx, reg, perms);

    REQUIRE(fsm.on_line("quit"));
    CHECK(ctx->closed);
    CHECK(reg->calls == 0);
    CHECK(ctx->sent == "bye\r\n");
}

TEST_CASE("missing ctx -> Internal error", "[protocol][telnet][admin]") {
    auto perms = std::make_shared<FakePerms>();
    auto reg   = std::make_shared<FakeRegistry>();
    pt::TelnetAdminFsm fsm(nullptr, reg, perms);

    auto r = fsm.on_connected();
    REQUIRE_FALSE(r);
    CHECK(r.error().code() == pvpgn::core::StatusCode::Internal);
}
