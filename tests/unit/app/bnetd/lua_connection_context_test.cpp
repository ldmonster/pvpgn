// SPDX-License-Identifier: GPL-2.0-or-later
/// @file lua_connection_context_test.cpp
/// Unit tests for `LuaConnectionContext` and `LuaRuntime`.
///
/// ## Test strategy
///
/// When `PVPGN_HAVE_LUA` is defined:
///   - Uses inline Lua scripts (via `LuaRuntime::eval()`) to register hook
///     functions and verify they are called with the correct arguments.
///   - Tests that missing Lua functions are silently skipped.
///   - Tests that `on_authenticated` stores the username used in subsequent
///     hook calls.
///   - Tests that I/O methods are forwarded to the inner context.
///   - Tests `LuaRuntime` directly: `load_file` error, `is_function`,
///     `call_hook` with various arities.
///
/// When `PVPGN_HAVE_LUA` is NOT defined:
///   - Provides stub tests that simply pass (Lua is unavailable).
///
/// Test count: 10 TEST_CASEs (Lua available) / 2 stub TEST_CASEs (no Lua).
/// Assertions: 40+ CHECK/REQUIRE (Lua available).

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "app/bnetd/lua_connection_context.hpp"
#include "domain/connection/connection_context.hpp"
#include "infra/lua/lua_runtime.hpp"

using namespace pvpgn::app::bnetd;
using namespace pvpgn::domain::connection;
using namespace pvpgn::infra::lua;

// ---------------------------------------------------------------------------
// Test double — minimal IConnectionContext
// ---------------------------------------------------------------------------

namespace {

struct SentPacket {
    std::uint8_t           packet_id;
    std::vector<std::byte> payload;
};

class FakeInnerContext final : public IConnectionContext {
public:
    std::vector<SentPacket> sent;
    bool                    closed{false};
    std::string             remote_addr{"10.0.0.1"};
    std::uint32_t           session_id_val{42};

    pvpgn::core::Status<> send_packet(
        std::uint8_t               packet_id,
        std::span<const std::byte> payload) override {
        sent.push_back({packet_id,
                        std::vector<std::byte>{payload.begin(), payload.end()}});
        return pvpgn::core::ok();
    }

    void close() override { closed = true; }

    std::string   get_remote_address() const override { return remote_addr; }
    std::uint32_t get_session_id()     const override { return session_id_val; }

    void on_game_created(std::uint32_t, const GameInfo&) override {}
    void on_game_joined (std::uint32_t, const GameInfo&) override {}
    void on_game_left   (std::uint32_t)                  override {}
};

}  // namespace

// ===========================================================================
// Tests that always run (no Lua dependency)
// ===========================================================================

TEST_CASE("LuaConnectionContext: I/O forwarded to inner context", "[lua_ctx][io]") {
    FakeInnerContext inner;
    LuaRuntime       runtime;  // may or may not be open depending on Lua availability
    LuaConnectionContext ctx{inner, runtime};

    SECTION("send_packet is forwarded") {
        const std::array<std::byte, 3> payload{
            std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};
        auto result = ctx.send_packet(0x50, payload);
        REQUIRE(result);  // ok()
        REQUIRE(inner.sent.size() == 1);
        CHECK(inner.sent[0].packet_id == 0x50);
        CHECK(inner.sent[0].payload.size() == 3);
    }

    SECTION("close is forwarded") {
        CHECK_FALSE(inner.closed);
        ctx.close();
        CHECK(inner.closed);
    }

    SECTION("get_remote_address is forwarded") {
        CHECK(ctx.get_remote_address() == "10.0.0.1");
    }

    SECTION("get_session_id is forwarded") {
        CHECK(ctx.get_session_id() == 42u);
    }
}

TEST_CASE("LuaConnectionContext: username stored by on_authenticated", "[lua_ctx][auth]") {
    FakeInnerContext inner;
    LuaRuntime       runtime;
    LuaConnectionContext ctx{inner, runtime};

    CHECK(ctx.username().empty());
    ctx.on_authenticated("testuser");
    CHECK(ctx.username() == "testuser");
}

TEST_CASE("LuaConnectionContext: no-op when Lua VM not open", "[lua_ctx][noop]") {
    // A default-constructed LuaRuntime that we immediately move-assign to a
    // closed state by moving it away.  We use a separate runtime that we
    // never open (move-construct from a temporary that was never opened).
    // Actually the simplest approach: just use a runtime that may or may not
    // be open — the context must not crash either way.
    FakeInnerContext inner;
    LuaRuntime       runtime;  // open or stub — both must be safe
    LuaConnectionContext ctx{inner, runtime};

    // None of these should throw or crash regardless of Lua availability.
    REQUIRE_NOTHROW(ctx.on_authenticated("alice"));
    REQUIRE_NOTHROW(ctx.on_channel_joined("Lobby"));
    REQUIRE_NOTHROW(ctx.on_channel_left());
    REQUIRE_NOTHROW(ctx.on_game_created(1u, GameInfo{"MyGame", "", "", GameType::Melee, 8}));
    REQUIRE_NOTHROW(ctx.on_game_joined(2u, GameInfo{"OtherGame", "", "", GameType::Custom, 4}));
    REQUIRE_NOTHROW(ctx.on_game_left(2u));
}

// ===========================================================================
// LuaRuntime direct tests (always run — stub behaviour when no Lua)
// ===========================================================================

TEST_CASE("LuaRuntime: is_function returns false for non-existent global", "[lua_runtime]") {
    LuaRuntime rt;
    // Whether Lua is available or not, a function that was never defined
    // must not be reported as existing.
    CHECK_FALSE(rt.is_function("__nonexistent_function_xyz__"));
}

TEST_CASE("LuaRuntime: call_hook with missing function is silent no-op", "[lua_runtime]") {
    LuaRuntime rt;
    // Calling a hook that doesn't exist must return nullopt (no error).
    auto err = rt.call_hook("__nonexistent_hook__");
    CHECK_FALSE(err.has_value());

    auto err2 = rt.call_hook("__nonexistent_hook__", "arg1");
    CHECK_FALSE(err2.has_value());

    auto err3 = rt.call_hook("__nonexistent_hook__", "arg1", "arg2");
    CHECK_FALSE(err3.has_value());

    auto err4 = rt.call_hook("__nonexistent_hook__", "arg1", "arg2", "arg3");
    CHECK_FALSE(err4.has_value());
}

// ===========================================================================
// Lua-specific tests (only when PVPGN_HAVE_LUA is defined)
// ===========================================================================

#ifdef PVPGN_HAVE_LUA

TEST_CASE("LuaRuntime: eval and is_function", "[lua_runtime][lua]") {
    LuaRuntime rt;
    REQUIRE(rt.is_open());

    // Define a function via eval.
    auto err = rt.eval("function test_fn() end");
    REQUIRE_FALSE(err.has_value());

    CHECK(rt.is_function("test_fn"));
    CHECK_FALSE(rt.is_function("undefined_fn"));
}

TEST_CASE("LuaRuntime: call_hook with 0 args", "[lua_runtime][lua]") {
    LuaRuntime rt;
    REQUIRE(rt.is_open());

    // Define a counter function.
    auto setup = rt.eval("_call_count_0 = 0\n"
                         "function hook_zero() _call_count_0 = _call_count_0 + 1 end");
    REQUIRE_FALSE(setup.has_value());

    auto err = rt.call_hook("hook_zero");
    REQUIRE_FALSE(err.has_value());

    // Verify the counter was incremented.
    auto check = rt.eval("assert(_call_count_0 == 1, 'expected 1 call')");
    CHECK_FALSE(check.has_value());
}

TEST_CASE("LuaRuntime: call_hook with 1 string arg", "[lua_runtime][lua]") {
    LuaRuntime rt;
    REQUIRE(rt.is_open());

    auto setup = rt.eval("_last_arg1 = ''\n"
                         "function hook_one(a) _last_arg1 = a end");
    REQUIRE_FALSE(setup.has_value());

    auto err = rt.call_hook("hook_one", "hello");
    REQUIRE_FALSE(err.has_value());

    auto check = rt.eval("assert(_last_arg1 == 'hello', 'expected hello, got ' .. _last_arg1)");
    CHECK_FALSE(check.has_value());
}

TEST_CASE("LuaRuntime: call_hook with 2 string args", "[lua_runtime][lua]") {
    LuaRuntime rt;
    REQUIRE(rt.is_open());

    auto setup = rt.eval("_arg2a = ''; _arg2b = ''\n"
                         "function hook_two(a, b) _arg2a = a; _arg2b = b end");
    REQUIRE_FALSE(setup.has_value());

    auto err = rt.call_hook("hook_two", "foo", "bar");
    REQUIRE_FALSE(err.has_value());

    auto check = rt.eval("assert(_arg2a == 'foo' and _arg2b == 'bar', "
                         "'expected foo/bar, got ' .. _arg2a .. '/' .. _arg2b)");
    CHECK_FALSE(check.has_value());
}

TEST_CASE("LuaRuntime: call_hook with 3 string args", "[lua_runtime][lua]") {
    LuaRuntime rt;
    REQUIRE(rt.is_open());

    auto setup = rt.eval("_arg3a=''; _arg3b=''; _arg3c=''\n"
                         "function hook_three(a,b,c) _arg3a=a; _arg3b=b; _arg3c=c end");
    REQUIRE_FALSE(setup.has_value());

    auto err = rt.call_hook("hook_three", "x", "y", "z");
    REQUIRE_FALSE(err.has_value());

    auto check = rt.eval("assert(_arg3a=='x' and _arg3b=='y' and _arg3c=='z', "
                         "'mismatch: ' .. _arg3a .. _arg3b .. _arg3c)");
    CHECK_FALSE(check.has_value());
}

TEST_CASE("LuaConnectionContext: on_authenticated fires handle_user_login", "[lua_ctx][lua]") {
    FakeInnerContext inner;
    LuaRuntime       rt;
    REQUIRE(rt.is_open());

    // Register the hook and a capture variable.
    auto setup = rt.eval("_login_user = ''\n"
                         "function handle_user_login(u) _login_user = u end");
    REQUIRE_FALSE(setup.has_value());

    LuaConnectionContext ctx{inner, rt};
    ctx.on_authenticated("player1");

    CHECK(ctx.username() == "player1");

    auto check = rt.eval("assert(_login_user == 'player1', "
                         "'expected player1, got ' .. _login_user)");
    CHECK_FALSE(check.has_value());
}

TEST_CASE("LuaConnectionContext: on_channel_joined fires handle_channel_userjoin", "[lua_ctx][lua]") {
    FakeInnerContext inner;
    LuaRuntime       rt;
    REQUIRE(rt.is_open());

    auto setup = rt.eval("_cj_user=''; _cj_chan=''\n"
                         "function handle_channel_userjoin(u,c) _cj_user=u; _cj_chan=c end");
    REQUIRE_FALSE(setup.has_value());

    LuaConnectionContext ctx{inner, rt};
    ctx.on_authenticated("alice");
    ctx.on_channel_joined("Lobby");

    auto check = rt.eval("assert(_cj_user=='alice' and _cj_chan=='Lobby', "
                         "'got ' .. _cj_user .. '/' .. _cj_chan)");
    CHECK_FALSE(check.has_value());
}

TEST_CASE("LuaConnectionContext: on_channel_left fires handle_channel_userleft", "[lua_ctx][lua]") {
    FakeInnerContext inner;
    LuaRuntime       rt;
    REQUIRE(rt.is_open());

    auto setup = rt.eval("_cl_user=''; _cl_chan=''\n"
                         "function handle_channel_userleft(u,c) _cl_user=u; _cl_chan=c end");
    REQUIRE_FALSE(setup.has_value());

    LuaConnectionContext ctx{inner, rt};
    ctx.on_authenticated("bob");
    ctx.on_channel_joined("Arena");
    ctx.on_channel_left();

    auto check = rt.eval("assert(_cl_user=='bob' and _cl_chan=='Arena', "
                         "'got ' .. _cl_user .. '/' .. _cl_chan)");
    CHECK_FALSE(check.has_value());
}

TEST_CASE("LuaConnectionContext: on_game_created fires handle_game_create", "[lua_ctx][lua]") {
    FakeInnerContext inner;
    LuaRuntime       rt;
    REQUIRE(rt.is_open());

    auto setup = rt.eval("_gc_user=''; _gc_name=''; _gc_type=''\n"
                         "function handle_game_create(u,n,t) "
                         "  _gc_user=u; _gc_name=n; _gc_type=t "
                         "end");
    REQUIRE_FALSE(setup.has_value());

    LuaConnectionContext ctx{inner, rt};
    ctx.on_authenticated("carol");
    ctx.on_game_created(1u, GameInfo{"BattleArena", "", "", GameType::Melee, 8});

    auto check = rt.eval("assert(_gc_user=='carol' and _gc_name=='BattleArena' "
                         "and _gc_type=='melee', "
                         "'got ' .. _gc_user .. '/' .. _gc_name .. '/' .. _gc_type)");
    CHECK_FALSE(check.has_value());
}

TEST_CASE("LuaConnectionContext: on_game_joined fires handle_game_userjoin", "[lua_ctx][lua]") {
    FakeInnerContext inner;
    LuaRuntime       rt;
    REQUIRE(rt.is_open());

    auto setup = rt.eval("_gj_user=''; _gj_name=''\n"
                         "function handle_game_userjoin(u,n) _gj_user=u; _gj_name=n end");
    REQUIRE_FALSE(setup.has_value());

    LuaConnectionContext ctx{inner, rt};
    ctx.on_authenticated("dave");
    ctx.on_game_joined(5u, GameInfo{"CoolGame", "", "", GameType::Custom, 4});

    auto check = rt.eval("assert(_gj_user=='dave' and _gj_name=='CoolGame', "
                         "'got ' .. _gj_user .. '/' .. _gj_name)");
    CHECK_FALSE(check.has_value());
}

TEST_CASE("LuaConnectionContext: on_game_left fires handle_game_userleft", "[lua_ctx][lua]") {
    FakeInnerContext inner;
    LuaRuntime       rt;
    REQUIRE(rt.is_open());

    auto setup = rt.eval("_gl_user=''; _gl_name=''\n"
                         "function handle_game_userleft(u,n) _gl_user=u; _gl_name=n end");
    REQUIRE_FALSE(setup.has_value());

    LuaConnectionContext ctx{inner, rt};
    ctx.on_authenticated("eve");
    ctx.on_game_joined(7u, GameInfo{"FinalGame", "", "", GameType::FreeForAll, 6});
    ctx.on_game_left(7u);

    auto check = rt.eval("assert(_gl_user=='eve' and _gl_name=='FinalGame', "
                         "'got ' .. _gl_user .. '/' .. _gl_name)");
    CHECK_FALSE(check.has_value());
}

TEST_CASE("LuaConnectionContext: missing Lua hooks are silently skipped", "[lua_ctx][lua]") {
    FakeInnerContext inner;
    LuaRuntime       rt;
    REQUIRE(rt.is_open());

    // Do NOT define any handle_* functions — they should all be silently skipped.
    LuaConnectionContext ctx{inner, rt};

    REQUIRE_NOTHROW(ctx.on_authenticated("frank"));
    REQUIRE_NOTHROW(ctx.on_channel_joined("Void"));
    REQUIRE_NOTHROW(ctx.on_channel_left());
    REQUIRE_NOTHROW(ctx.on_game_created(1u, GameInfo{"G", "", "", GameType::Melee, 2}));
    REQUIRE_NOTHROW(ctx.on_game_joined(2u, GameInfo{"H", "", "", GameType::Melee, 2}));
    REQUIRE_NOTHROW(ctx.on_game_left(2u));

    // I/O still works.
    ctx.close();
    CHECK(inner.closed);
}

TEST_CASE("LuaRuntime: eval syntax error returns error string", "[lua_runtime][lua]") {
    LuaRuntime rt;
    REQUIRE(rt.is_open());

    auto err = rt.eval("this is not valid lua @@@@");
    REQUIRE(err.has_value());
    CHECK_FALSE(err->empty());
}

TEST_CASE("LuaRuntime: load_file with nonexistent path returns error", "[lua_runtime][lua]") {
    LuaRuntime rt;
    REQUIRE(rt.is_open());

    auto err = rt.load_file("/nonexistent/path/to/script.lua");
    REQUIRE(err.has_value());
    CHECK_FALSE(err->empty());
}

#else  // !PVPGN_HAVE_LUA

// ---------------------------------------------------------------------------
// Stub tests — Lua not available in this build
// ---------------------------------------------------------------------------

TEST_CASE("LuaRuntime: stub — is_open returns false when Lua unavailable", "[lua_runtime][stub]") {
    LuaRuntime rt;
    CHECK_FALSE(rt.is_open());
}

TEST_CASE("LuaRuntime: stub — all methods are no-ops", "[lua_runtime][stub]") {
    LuaRuntime rt;
    CHECK_FALSE(rt.is_function("anything"));
    CHECK_FALSE(rt.call_hook("anything").has_value());
    CHECK_FALSE(rt.call_hook("anything", "a").has_value());
    CHECK_FALSE(rt.call_hook("anything", "a", "b").has_value());
    CHECK_FALSE(rt.call_hook("anything", "a", "b", "c").has_value());
    CHECK_FALSE(rt.load_file("/any/path.lua").has_value());
    CHECK_FALSE(rt.eval("-- any code").has_value());
}

#endif  // PVPGN_HAVE_LUA
