#pragma once

#include <sol/sol.hpp>
#include <memory>

namespace pvpgn::infra::scripting {

// Forward declarations
class PluginContext;

/// Compatibility shim for legacy Lua scripts
/// Translates old t_account/t_connection style API to new v3 API
class LegacyCompatShim {
public:
    /// Setup legacy API bindings in Lua state
    /// This allows old scripts to work with the new infrastructure
    static void setup_legacy_api(sol::state& lua, PluginContext& context);
    
private:
    /// Setup legacy account userdata and functions
    static void setup_account_api(sol::state& lua, PluginContext& context);
    
    /// Setup legacy connection userdata and functions
    static void setup_connection_api(sol::state& lua, PluginContext& context);
    
    /// Setup legacy channel userdata and functions
    static void setup_channel_api(sol::state& lua, PluginContext& context);
    
    /// Setup legacy game userdata and functions
    static void setup_game_api(sol::state& lua, PluginContext& context);
    
    /// Setup legacy message functions
    static void setup_message_api(sol::state& lua, PluginContext& context);
    
    /// Setup legacy command functions
    static void setup_command_api(sol::state& lua, PluginContext& context);
};

} // namespace pvpgn::infra::scripting
