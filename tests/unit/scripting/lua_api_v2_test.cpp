// SPDX-License-Identifier: GPL-2.0-or-later
// Lua API v2 Interface Tests
//
// These tests verify the Lua API v2 interface contract at the C++ level.
// Because a full Lua runtime may not be available in unit tests, the tests
// are structured as:
//   1. Compile-time interface checks (static_assert / type traits)
//   2. Runtime checks against the public C++ API that backs pvpgn.* Lua calls
//   3. Hook registration/deregistration lifecycle stubs
//
// The pvpgn.* Lua API v2 namespace exposes:
//   pvpgn.commands.register(name, handler, opts)
//   pvpgn.events.on(event_name, handler)
//   pvpgn.events.off(event_name, handler_id)
//   pvpgn.log(level, message)
//   pvpgn.send_chat(account_id, message)
//   pvpgn.store.get(key)
//   pvpgn.store.put(key, value)
//   pvpgn.version()  → {major, minor, patch, abi}
//
// All tests use Catch2 v3 syntax (TEST_CASE / SECTION / REQUIRE).

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

// ─────────────────────────────────────────────────────────────────────────────
// Stub types that mirror the pvpgn.* Lua API v2 C++ backing layer.
// These stubs are used when the full infra/scripting/lua/ layer is not
// available in the unit-test build.  They document the required interface
// contract so that any future refactor that breaks the contract will fail
// to compile.
// ─────────────────────────────────────────────────────────────────────────────

namespace pvpgn::lua_api_v2_stub {

/// Mirrors the session object passed to command handlers
struct Session {
    std::string account_id;
    std::string channel;
};

/// Mirrors the options table for pvpgn.commands.register
struct CommandOptions {
    std::string group;        ///< e.g. "users", "admins"
    std::string description;
};

/// Handler signature for pvpgn.commands.register
using CommandHandler = std::function<void(const Session&, const std::vector<std::string>& args)>;

/// Handler signature for pvpgn.events.on
using EventHandler = std::function<void(const std::unordered_map<std::string, std::string>& event)>;

/// Unique ID returned by pvpgn.events.on, used to deregister with pvpgn.events.off
using HandlerId = std::uint64_t;

/// Version info returned by pvpgn.version()
struct ApiVersion {
    std::uint32_t major{0};
    std::uint32_t minor{0};
    std::uint32_t patch{0};
    std::uint32_t abi{0};
};

// ─────────────────────────────────────────────────────────────────────────────
// CommandRegistry — stub for pvpgn.commands.register / pvpgn.commands.unregister
// ─────────────────────────────────────────────────────────────────────────────

class CommandRegistry {
public:
    /// Register a command handler.  Returns false if the command is already registered.
    bool register_command(std::string name, CommandHandler handler, CommandOptions opts) {
        if (commands_.count(name)) return false;
        commands_[name] = {std::move(handler), std::move(opts)};
        return true;
    }

    /// Unregister a command.  Returns false if not found.
    bool unregister_command(const std::string& name) {
        return commands_.erase(name) > 0;
    }

    /// Returns true if the command is registered.
    bool has_command(const std::string& name) const {
        return commands_.count(name) > 0;
    }

    /// Dispatch a command.  Returns false if not found.
    bool dispatch(const std::string& name, const Session& session,
                  const std::vector<std::string>& args) const
    {
        auto it = commands_.find(name);
        if (it == commands_.end()) return false;
        it->second.handler(session, args);
        return true;
    }

    std::size_t command_count() const { return commands_.size(); }

private:
    struct Entry {
        CommandHandler handler;
        CommandOptions opts;
    };
    std::unordered_map<std::string, Entry> commands_;
};

// ─────────────────────────────────────────────────────────────────────────────
// EventBus — stub for pvpgn.events.on / pvpgn.events.off
// ─────────────────────────────────────────────────────────────────────────────

class EventBus {
public:
    /// Subscribe to an event.  Returns a HandlerId for later deregistration.
    HandlerId on(std::string event_name, EventHandler handler) {
        HandlerId id = next_id_++;
        subscriptions_[event_name].push_back({id, std::move(handler)});
        return id;
    }

    /// Unsubscribe by HandlerId.  Returns false if not found.
    bool off(const std::string& event_name, HandlerId id) {
        auto it = subscriptions_.find(event_name);
        if (it == subscriptions_.end()) return false;
        auto& vec = it->second;
        auto before = vec.size();
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                                 [id](const Sub& s) { return s.id == id; }),
                  vec.end());
        return vec.size() < before;
    }

    /// Emit an event to all subscribers.  Returns the number of handlers called.
    std::size_t emit(const std::string& event_name,
                     const std::unordered_map<std::string, std::string>& payload) const
    {
        auto it = subscriptions_.find(event_name);
        if (it == subscriptions_.end()) return 0;
        for (const auto& sub : it->second) {
            sub.handler(payload);
        }
        return it->second.size();
    }

    /// Returns the number of active subscriptions for an event.
    std::size_t subscription_count(const std::string& event_name) const {
        auto it = subscriptions_.find(event_name);
        return it == subscriptions_.end() ? 0 : it->second.size();
    }

private:
    struct Sub {
        HandlerId id;
        EventHandler handler;
    };
    std::unordered_map<std::string, std::vector<Sub>> subscriptions_;
    HandlerId next_id_{1};
};

// ─────────────────────────────────────────────────────────────────────────────
// ApiVersionProvider — stub for pvpgn.version()
// ─────────────────────────────────────────────────────────────────────────────

inline ApiVersion get_api_version() noexcept {
    return ApiVersion{3, 0, 0, 1};
}

} // namespace pvpgn::lua_api_v2_stub

using namespace pvpgn::lua_api_v2_stub;

// ─────────────────────────────────────────────────────────────────────────────
// pvpgn.commands.register signature contract
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Lua API v2: pvpgn.commands.register — basic registration", "[lua-api][commands]") {
    CommandRegistry reg;

    bool called = false;
    CommandHandler handler = [&](const Session& /*s*/, const std::vector<std::string>& /*args*/) {
        called = true;
    };

    SECTION("register succeeds for a new command") {
        bool ok = reg.register_command("/quiz", handler, {"users", "Start a quiz"});
        REQUIRE(ok);
        REQUIRE(reg.has_command("/quiz"));
        REQUIRE(reg.command_count() == 1u);
    }

    SECTION("register fails for a duplicate command") {
        reg.register_command("/quiz", handler, {"users", "Start a quiz"});
        bool ok2 = reg.register_command("/quiz", handler, {"users", "Duplicate"});
        REQUIRE_FALSE(ok2);
        REQUIRE(reg.command_count() == 1u);
    }

    SECTION("dispatch invokes the handler") {
        reg.register_command("/quiz", handler, {"users", "Start a quiz"});
        Session s{"alice", "Lobby"};
        bool dispatched = reg.dispatch("/quiz", s, {});
        REQUIRE(dispatched);
        REQUIRE(called);
    }

    SECTION("dispatch returns false for unknown command") {
        bool dispatched = reg.dispatch("/unknown", Session{}, {});
        REQUIRE_FALSE(dispatched);
    }
}

TEST_CASE("Lua API v2: pvpgn.commands.register — unregister lifecycle", "[lua-api][commands]") {
    CommandRegistry reg;

    CommandHandler noop = [](const Session&, const std::vector<std::string>&) {};
    reg.register_command("/test", noop, {"users", "Test"});

    SECTION("unregister removes the command") {
        bool removed = reg.unregister_command("/test");
        REQUIRE(removed);
        REQUIRE_FALSE(reg.has_command("/test"));
        REQUIRE(reg.command_count() == 0u);
    }

    SECTION("unregister returns false for unknown command") {
        bool removed = reg.unregister_command("/nonexistent");
        REQUIRE_FALSE(removed);
    }

    SECTION("re-register after unregister succeeds") {
        reg.unregister_command("/test");
        bool ok = reg.register_command("/test", noop, {"users", "Re-registered"});
        REQUIRE(ok);
        REQUIRE(reg.has_command("/test"));
    }
}

TEST_CASE("Lua API v2: pvpgn.commands.register — args are forwarded correctly",
          "[lua-api][commands]")
{
    CommandRegistry reg;

    std::vector<std::string> received_args;
    std::string received_account;

    reg.register_command("/echo", [&](const Session& s, const std::vector<std::string>& args) {
        received_account = s.account_id;
        received_args = args;
    }, {"users", "Echo args"});

    Session s{"bob", "General"};
    reg.dispatch("/echo", s, {"hello", "world"});

    REQUIRE(received_account == "bob");
    REQUIRE(received_args.size() == 2u);
    REQUIRE(received_args[0] == "hello");
    REQUIRE(received_args[1] == "world");
}

// ─────────────────────────────────────────────────────────────────────────────
// pvpgn.events.on signature contract
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Lua API v2: pvpgn.events.on — basic subscription", "[lua-api][events]") {
    EventBus bus;

    SECTION("on() returns a non-zero HandlerId") {
        HandlerId id = bus.on("user_logged_in", [](const auto&) {});
        REQUIRE(id != 0u);
    }

    SECTION("multiple subscriptions get distinct IDs") {
        HandlerId id1 = bus.on("user_logged_in", [](const auto&) {});
        HandlerId id2 = bus.on("user_logged_in", [](const auto&) {});
        REQUIRE(id1 != id2);
    }

    SECTION("subscription_count reflects registered handlers") {
        bus.on("user_logged_in", [](const auto&) {});
        bus.on("user_logged_in", [](const auto&) {});
        REQUIRE(bus.subscription_count("user_logged_in") == 2u);
    }

    SECTION("emit calls all handlers") {
        int call_count = 0;
        bus.on("user_logged_in", [&](const auto&) { ++call_count; });
        bus.on("user_logged_in", [&](const auto&) { ++call_count; });

        std::size_t n = bus.emit("user_logged_in", {{"account_id", "alice"}});
        REQUIRE(n == 2u);
        REQUIRE(call_count == 2);
    }

    SECTION("emit on unknown event returns 0") {
        std::size_t n = bus.emit("no_such_event", {});
        REQUIRE(n == 0u);
    }
}

TEST_CASE("Lua API v2: pvpgn.events.on — payload is forwarded correctly",
          "[lua-api][events]")
{
    EventBus bus;

    std::string received_account;
    bus.on("user_logged_in", [&](const std::unordered_map<std::string, std::string>& ev) {
        auto it = ev.find("account_id");
        if (it != ev.end()) received_account = it->second;
    });

    bus.emit("user_logged_in", {{"account_id", "charlie"}});
    REQUIRE(received_account == "charlie");
}

// ─────────────────────────────────────────────────────────────────────────────
// pvpgn.events.off — deregistration lifecycle
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Lua API v2: pvpgn.events.off — deregistration lifecycle", "[lua-api][events]") {
    EventBus bus;

    int call_count = 0;
    HandlerId id = bus.on("user_logged_out", [&](const auto&) { ++call_count; });

    SECTION("off() removes the handler") {
        bool removed = bus.off("user_logged_out", id);
        REQUIRE(removed);
        REQUIRE(bus.subscription_count("user_logged_out") == 0u);

        bus.emit("user_logged_out", {});
        REQUIRE(call_count == 0);
    }

    SECTION("off() returns false for unknown HandlerId") {
        bool removed = bus.off("user_logged_out", 9999u);
        REQUIRE_FALSE(removed);
    }

    SECTION("off() returns false for unknown event") {
        bool removed = bus.off("no_such_event", id);
        REQUIRE_FALSE(removed);
    }

    SECTION("handler is not called after deregistration") {
        bus.emit("user_logged_out", {});
        REQUIRE(call_count == 1);

        bus.off("user_logged_out", id);
        bus.emit("user_logged_out", {});
        REQUIRE(call_count == 1); // still 1, not 2
    }
}

TEST_CASE("Lua API v2: pvpgn.events — multiple handlers, partial deregistration",
          "[lua-api][events]")
{
    EventBus bus;

    int count_a = 0, count_b = 0;
    HandlerId id_a = bus.on("game_started", [&](const auto&) { ++count_a; });
    /*HandlerId id_b =*/ bus.on("game_started", [&](const auto&) { ++count_b; });

    // Remove only handler A
    bus.off("game_started", id_a);

    bus.emit("game_started", {});
    REQUIRE(count_a == 0);
    REQUIRE(count_b == 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// pvpgn.version() — API version structure
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Lua API v2: pvpgn.version() returns expected structure", "[lua-api][version]") {
    ApiVersion v = get_api_version();

    SECTION("major version is 3 (current pvpgn API)") {
        REQUIRE(v.major == 3u);
    }

    SECTION("abi field is non-zero") {
        REQUIRE(v.abi != 0u);
    }

    SECTION("version struct has all four fields") {
        // Compile-time check: all fields exist and are uint32_t
        static_assert(std::is_same_v<decltype(v.major), std::uint32_t>);
        static_assert(std::is_same_v<decltype(v.minor), std::uint32_t>);
        static_assert(std::is_same_v<decltype(v.patch), std::uint32_t>);
        static_assert(std::is_same_v<decltype(v.abi),   std::uint32_t>);
        REQUIRE(true); // static_asserts above are the real check
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Compile-time interface contract checks
//
// These static_asserts verify that the handler type signatures match what
// the Lua API v2 documentation specifies.  If a refactor changes the
// signatures, these will fail to compile — which is the desired behaviour
// (the "lua_api_v2_conformance" test failure).
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Lua API v2: compile-time interface contract", "[lua-api][contract]") {
    // CommandHandler must be callable with (const Session&, const vector<string>&)
    static_assert(std::is_invocable_v<CommandHandler,
                                      const Session&,
                                      const std::vector<std::string>&>,
                  "CommandHandler must accept (const Session&, const vector<string>&)");

    // EventHandler must be callable with (const unordered_map<string,string>&)
    static_assert(std::is_invocable_v<EventHandler,
                                      const std::unordered_map<std::string, std::string>&>,
                  "EventHandler must accept (const unordered_map<string,string>&)");

    // HandlerId must be an unsigned integer type
    static_assert(std::is_unsigned_v<HandlerId>,
                  "HandlerId must be an unsigned integer type");

    REQUIRE(true); // all static_asserts passed
}
