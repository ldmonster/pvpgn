// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file lua_runtime.hpp
/// Clean C++20 wrapper around a Lua VM (`lua_State*`) lifecycle.
///
/// ## Design
///
/// `LuaRuntime` is a RAII owner of a single `lua_State*`.  It provides:
///   - `load_file(path)`   — load and execute a Lua source file
///   - `call_hook(name, args...)` — call a named global Lua function
///   - `is_function(name)` — check whether a global Lua function exists
///
/// All methods are no-ops (returning empty optionals / false) when Lua is
/// not available (`PVPGN_HAVE_LUA` not defined).
///
/// ## Error handling
///
/// No exceptions are thrown.  Errors are returned as
/// `std::optional<std::string>` (present = error message, absent = success).
///
/// ## Thread safety
///
/// A single `LuaRuntime` instance is NOT thread-safe.  The caller is
/// responsible for serialising access (e.g. via a strand or mutex).
///
/// ## C++20 conventions
///   - RAII; move-only
///   - `[[nodiscard]]` on all error-returning methods
///   - `extern "C"` includes for Lua headers

#include <optional>
#include <string>
#include <string_view>

namespace pvpgn::infra::lua {

// ---------------------------------------------------------------------------
// LuaRuntime
// ---------------------------------------------------------------------------

/// RAII wrapper around a Lua VM (`lua_State*`).
///
/// When `PVPGN_HAVE_LUA` is not defined the entire class compiles to a
/// stub whose methods are all no-ops.
class LuaRuntime {
public:
    /// Construct and open a new Lua VM.
    ///
    /// On success the standard Lua libraries are opened.
    /// If Lua is not available this is a no-op.
    LuaRuntime();

    /// Destroy the Lua VM (calls `lua_close`).
    ~LuaRuntime();

    // Move-only (the lua_State* cannot be shared).
    LuaRuntime(const LuaRuntime&)            = delete;
    LuaRuntime& operator=(const LuaRuntime&) = delete;
    LuaRuntime(LuaRuntime&&) noexcept;
    LuaRuntime& operator=(LuaRuntime&&) noexcept;

    // -----------------------------------------------------------------------
    // File loading
    // -----------------------------------------------------------------------

    /// Load and execute a Lua source file.
    ///
    /// @param path  Path to the `.lua` file.
    /// @return      `std::nullopt` on success; error message string on failure.
    [[nodiscard]] std::optional<std::string> load_file(std::string_view path);

    // -----------------------------------------------------------------------
    // Hook invocation
    // -----------------------------------------------------------------------

    /// Check whether a global Lua function with the given name exists.
    ///
    /// @param name  Global function name (e.g. `"handle_user_login"`).
    /// @return      `true` if the global is a function; `false` otherwise.
    [[nodiscard]] bool is_function(std::string_view name) const;

    /// Call a global Lua function with zero arguments.
    ///
    /// Silently skips if the function does not exist.
    /// @return `std::nullopt` on success; error message on Lua error.
    [[nodiscard]] std::optional<std::string> call_hook(std::string_view name);

    /// Call a global Lua function with one string argument.
    [[nodiscard]] std::optional<std::string> call_hook(std::string_view name,
                                                        std::string_view arg1);

    /// Call a global Lua function with two string arguments.
    [[nodiscard]] std::optional<std::string> call_hook(std::string_view name,
                                                        std::string_view arg1,
                                                        std::string_view arg2);

    /// Call a global Lua function with three string arguments.
    [[nodiscard]] std::optional<std::string> call_hook(std::string_view name,
                                                        std::string_view arg1,
                                                        std::string_view arg2,
                                                        std::string_view arg3);

    /// Execute an arbitrary Lua string (for testing / inline scripts).
    ///
    /// @param code  Lua source code to evaluate.
    /// @return      `std::nullopt` on success; error message on failure.
    [[nodiscard]] std::optional<std::string> eval(std::string_view code);

    /// Returns `true` if the Lua VM was successfully initialised.
    [[nodiscard]] bool is_open() const noexcept;

private:
    // Opaque pointer to lua_State — avoids exposing Lua headers to consumers.
    // When PVPGN_HAVE_LUA is not defined this is always nullptr.
    void* state_{nullptr};  // actually lua_State*

    // Internal helpers (defined in lua_runtime.cpp, guarded by PVPGN_HAVE_LUA)
    [[nodiscard]] std::optional<std::string> do_call(int n_args);
};

}  // namespace pvpgn::infra::lua
