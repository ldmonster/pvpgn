// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file sol2_script_host.cpp
 * @brief sol2 / Lua 5.4 implementation of IScriptHost.
 */

#include "infra/scripting/sol2_script_host.hpp"

// sol2 must be included before any Lua headers to avoid ODR issues.
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace pvpgn::infra::scripting {

// ============================================================================
// Pimpl — hides sol2 types from the public header
// ============================================================================

struct Sol2ScriptHost::Impl {
    sol::state lua;

    explicit Impl() {
        // Open all standard Lua libraries (io, os, math, string, table, …).
        lua.open_libraries(
            sol::lib::base,
            sol::lib::coroutine,
            sol::lib::string,
            sol::lib::table,
            sol::lib::math,
            sol::lib::io,
            sol::lib::os,
            sol::lib::package
        );
    }
};

// ============================================================================
// Sol2ScriptHost
// ============================================================================

Sol2ScriptHost::Sol2ScriptHost()
    : impl_(std::make_unique<Impl>())
{}

Sol2ScriptHost::~Sol2ScriptHost() = default;

// ---- load_file --------------------------------------------------------------

void Sol2ScriptHost::load_file(std::string_view path) {
    try {
        impl_->lua.script_file(std::string(path));
    } catch (const sol::error& e) {
        throw std::runtime_error(
            std::string("Sol2ScriptHost::load_file('") + std::string(path) +
            "'): " + e.what());
    }
}

// ---- exec -------------------------------------------------------------------

void Sol2ScriptHost::exec(std::string_view code) {
    try {
        impl_->lua.script(std::string(code));
    } catch (const sol::error& e) {
        throw std::runtime_error(
            std::string("Sol2ScriptHost::exec: ") + e.what());
    }
}

// ---- register_function ------------------------------------------------------

void Sol2ScriptHost::register_function(std::string_view name,
                                       application::ports::ScriptEventHandler handler) {
    // Capture handler by value so it outlives this call.
    impl_->lua.set_function(std::string(name),
        [h = std::move(handler), n = std::string(name)]
        (std::string_view payload) -> std::string {
            return h(n, payload);
        });
}

// ---- dispatch_event ---------------------------------------------------------

std::string Sol2ScriptHost::dispatch_event(std::string_view event,
                                           std::string_view payload) {
    const std::string event_str(event);

    // Check if the global exists and is callable.
    sol::object obj = impl_->lua[event_str].get<sol::object>();
    if (!obj.valid() || obj.get_type() != sol::type::function) {
        return {};
    }

    try {
        sol::protected_function fn =
            impl_->lua[event_str].get<sol::protected_function>();
        sol::protected_function_result result = fn(std::string(payload));

        if (!result.valid()) {
            sol::error err = result;
            throw std::runtime_error(
                std::string("Sol2ScriptHost::dispatch_event('") + event_str +
                "'): Lua error: " + err.what());
        }

        // Return the result as a string if the handler returned one.
        if (result.return_count() > 0) {
            sol::optional<std::string> ret = result.get<sol::optional<std::string>>(0);
            if (ret) return *ret;
        }
        return {};

    } catch (const sol::error& e) {
        throw std::runtime_error(
            std::string("Sol2ScriptHost::dispatch_event('") + event_str +
            "'): " + e.what());
    }
}

// ---- has_handler ------------------------------------------------------------

bool Sol2ScriptHost::has_handler(std::string_view event) const noexcept {
    try {
        sol::object obj =
            impl_->lua[std::string(event)].get<sol::object>();
        return obj.valid() && obj.get_type() == sol::type::function;
    } catch (...) {
        return false;
    }
}

} // namespace pvpgn::infra::scripting
