// SPDX-License-Identifier: GPL-2.0-or-later

/// @file lua_runtime.cpp
/// Implementation of `LuaRuntime` — RAII wrapper around a Lua VM.
///
/// The entire implementation is guarded by `#ifdef PVPGN_HAVE_LUA`.
/// When Lua is not available all methods compile to no-ops that return
/// empty optionals / false / nullptr.

#include "infra/lua/lua_runtime.hpp"

#ifdef PVPGN_HAVE_LUA

// Use extern "C" to include Lua C headers from C++ code.
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <cstring>

#endif  // PVPGN_HAVE_LUA

namespace pvpgn::infra::lua {

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

LuaRuntime::LuaRuntime() {
#ifdef PVPGN_HAVE_LUA
    lua_State* L = luaL_newstate();
    if (L) {
        luaL_openlibs(L);
        state_ = static_cast<void*>(L);
    }
#endif
}

LuaRuntime::~LuaRuntime() {
#ifdef PVPGN_HAVE_LUA
    if (state_) {
        lua_close(static_cast<lua_State*>(state_));
        state_ = nullptr;
    }
#endif
}

// ---------------------------------------------------------------------------
// Move semantics
// ---------------------------------------------------------------------------

LuaRuntime::LuaRuntime(LuaRuntime&& other) noexcept
    : state_(other.state_) {
    other.state_ = nullptr;
}

LuaRuntime& LuaRuntime::operator=(LuaRuntime&& other) noexcept {
    if (this != &other) {
#ifdef PVPGN_HAVE_LUA
        if (state_) {
            lua_close(static_cast<lua_State*>(state_));
        }
#endif
        state_       = other.state_;
        other.state_ = nullptr;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// is_open
// ---------------------------------------------------------------------------

bool LuaRuntime::is_open() const noexcept {
    return state_ != nullptr;
}

// ---------------------------------------------------------------------------
// load_file
// ---------------------------------------------------------------------------

std::optional<std::string> LuaRuntime::load_file(std::string_view path) {
#ifdef PVPGN_HAVE_LUA
    if (!state_) return "LuaRuntime: VM not initialised";

    lua_State* L = static_cast<lua_State*>(state_);

    // luaL_loadfile expects a null-terminated string.
    const std::string path_str{path};

    if (luaL_loadfile(L, path_str.c_str()) != 0 ||
        lua_pcall(L, 0, 0, 0) != 0) {
        // Pop the error message from the stack.
        std::string err;
        if (lua_isstring(L, -1)) {
            err = lua_tostring(L, -1);
        } else {
            err = "LuaRuntime: unknown error loading file: ";
            err += path_str;
        }
        lua_pop(L, 1);
        return err;
    }
    return std::nullopt;
#else
    (void)path;
    return std::nullopt;
#endif
}

// ---------------------------------------------------------------------------
// eval
// ---------------------------------------------------------------------------

std::optional<std::string> LuaRuntime::eval(std::string_view code) {
#ifdef PVPGN_HAVE_LUA
    if (!state_) return "LuaRuntime: VM not initialised";

    lua_State* L = static_cast<lua_State*>(state_);
    const std::string code_str{code};

    if (luaL_loadstring(L, code_str.c_str()) != 0 ||
        lua_pcall(L, 0, 0, 0) != 0) {
        std::string err;
        if (lua_isstring(L, -1)) {
            err = lua_tostring(L, -1);
        } else {
            err = "LuaRuntime: unknown error evaluating code";
        }
        lua_pop(L, 1);
        return err;
    }
    return std::nullopt;
#else
    (void)code;
    return std::nullopt;
#endif
}

// ---------------------------------------------------------------------------
// is_function
// ---------------------------------------------------------------------------

bool LuaRuntime::is_function(std::string_view name) const {
#ifdef PVPGN_HAVE_LUA
    if (!state_) return false;

    lua_State* L = static_cast<lua_State*>(state_);
    const std::string name_str{name};

    lua_getglobal(L, name_str.c_str());
    const bool result = (lua_type(L, -1) == LUA_TFUNCTION);
    lua_pop(L, 1);
    return result;
#else
    (void)name;
    return false;
#endif
}

// ---------------------------------------------------------------------------
// do_call — internal helper: call function already on stack with n_args args
// ---------------------------------------------------------------------------

std::optional<std::string> LuaRuntime::do_call(int n_args) {
#ifdef PVPGN_HAVE_LUA
    lua_State* L = static_cast<lua_State*>(state_);

    // Stack layout before call:
    //   [-n_args-1]  function
    //   [-n_args...-1]  arguments
    if (lua_pcall(L, n_args, 0, 0) != 0) {
        std::string err;
        if (lua_isstring(L, -1)) {
            err = lua_tostring(L, -1);
        } else {
            err = "LuaRuntime: unknown error in hook call";
        }
        lua_pop(L, 1);
        return err;
    }
    return std::nullopt;
#else
    (void)n_args;
    return std::nullopt;
#endif
}

// ---------------------------------------------------------------------------
// call_hook overloads
// ---------------------------------------------------------------------------

std::optional<std::string> LuaRuntime::call_hook(std::string_view name) {
#ifdef PVPGN_HAVE_LUA
    if (!state_) return std::nullopt;
    if (!is_function(name)) return std::nullopt;

    lua_State* L = static_cast<lua_State*>(state_);
    const std::string name_str{name};
    lua_getglobal(L, name_str.c_str());
    return do_call(0);
#else
    (void)name;
    return std::nullopt;
#endif
}

std::optional<std::string> LuaRuntime::call_hook(std::string_view name,
                                                   std::string_view arg1) {
#ifdef PVPGN_HAVE_LUA
    if (!state_) return std::nullopt;
    if (!is_function(name)) return std::nullopt;

    lua_State* L = static_cast<lua_State*>(state_);
    const std::string name_str{name};
    lua_getglobal(L, name_str.c_str());
    lua_pushlstring(L, arg1.data(), arg1.size());
    return do_call(1);
#else
    (void)name;
    (void)arg1;
    return std::nullopt;
#endif
}

std::optional<std::string> LuaRuntime::call_hook(std::string_view name,
                                                   std::string_view arg1,
                                                   std::string_view arg2) {
#ifdef PVPGN_HAVE_LUA
    if (!state_) return std::nullopt;
    if (!is_function(name)) return std::nullopt;

    lua_State* L = static_cast<lua_State*>(state_);
    const std::string name_str{name};
    lua_getglobal(L, name_str.c_str());
    lua_pushlstring(L, arg1.data(), arg1.size());
    lua_pushlstring(L, arg2.data(), arg2.size());
    return do_call(2);
#else
    (void)name;
    (void)arg1;
    (void)arg2;
    return std::nullopt;
#endif
}

std::optional<std::string> LuaRuntime::call_hook(std::string_view name,
                                                   std::string_view arg1,
                                                   std::string_view arg2,
                                                   std::string_view arg3) {
#ifdef PVPGN_HAVE_LUA
    if (!state_) return std::nullopt;
    if (!is_function(name)) return std::nullopt;

    lua_State* L = static_cast<lua_State*>(state_);
    const std::string name_str{name};
    lua_getglobal(L, name_str.c_str());
    lua_pushlstring(L, arg1.data(), arg1.size());
    lua_pushlstring(L, arg2.data(), arg2.size());
    lua_pushlstring(L, arg3.data(), arg3.size());
    return do_call(3);
#else
    (void)name;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    return std::nullopt;
#endif
}

}  // namespace pvpgn::infra::lua
