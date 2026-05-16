#include "infra/scripting/lua/legacy_compat_shim.hpp"

namespace pvpgn::infra::scripting {

void LegacyCompatShim::setup_legacy_api(sol::state& lua, PluginContext& context)
{
    // Setup all legacy API components
    setup_account_api(lua, context);
    setup_connection_api(lua, context);
    setup_channel_api(lua, context);
    setup_game_api(lua, context);
    setup_message_api(lua, context);
    setup_command_api(lua, context);
}

void LegacyCompatShim::setup_account_api(sol::state& lua, PluginContext& context)
{
    // Legacy account functions
    // These translate to new v3 API calls
    
    // account_get_name(account) -> string
    lua["account_get_name"] = [&context](sol::object account) -> std::string {
        // TODO: Implement translation to new API
        return "";
    };
    
    // account_get_auth_command_groups(account_name) -> int
    lua["account_get_auth_command_groups"] = [&context](const std::string& name) -> int {
        // TODO: Implement translation to new API
        return 0;
    };
    
    // account_get_email(account) -> string
    lua["account_get_email"] = [&context](sol::object account) -> std::string {
        // TODO: Implement translation to new API
        return "";
    };
    
    // account_get_sex(account) -> string
    lua["account_get_sex"] = [&context](sol::object account) -> std::string {
        // TODO: Implement translation to new API
        return "";
    };
    
    // account_get_location(account) -> string
    lua["account_get_location"] = [&context](sol::object account) -> std::string {
        // TODO: Implement translation to new API
        return "";
    };
    
    // account_get_description(account) -> string
    lua["account_get_description"] = [&context](sol::object account) -> std::string {
        // TODO: Implement translation to new API
        return "";
    };
}

void LegacyCompatShim::setup_connection_api(sol::state& lua, PluginContext& context)
{
    // Legacy connection functions
    
    // connection_get_account(connection) -> account
    lua["connection_get_account"] = [&context](sol::object conn) -> sol::object {
        // TODO: Implement translation to new API
        return sol::nil;
    };
    
    // connection_get_ip(connection) -> string
    lua["connection_get_ip"] = [&context](sol::object conn) -> std::string {
        // TODO: Implement translation to new API
        return "";
    };
}

void LegacyCompatShim::setup_channel_api(sol::state& lua, PluginContext& context)
{
    // Legacy channel functions
    
    // channel_get_name(channel) -> string
    lua["channel_get_name"] = [&context](sol::object chan) -> std::string {
        // TODO: Implement translation to new API
        return "";
    };
    
    // channel_get_members(channel) -> table
    lua["channel_get_members"] = [&context](sol::object chan) -> sol::table {
        // TODO: Implement translation to new API
        return sol::table();
    };
}

void LegacyCompatShim::setup_game_api(sol::state& lua, PluginContext& context)
{
    // Legacy game functions
    
    // game_get_host(game) -> account
    lua["game_get_host"] = [&context](sol::object game) -> sol::object {
        // TODO: Implement translation to new API
        return sol::nil;
    };
    
    // game_get_players(game) -> table
    lua["game_get_players"] = [&context](sol::object game) -> sol::table {
        // TODO: Implement translation to new API
        return sol::table();
    };
}

void LegacyCompatShim::setup_message_api(sol::state& lua, PluginContext& context)
{
    // Legacy message constants and functions
    
    // Message types
    lua["message_type_talk"] = 0;
    lua["message_type_emote"] = 1;
    lua["message_type_whisper"] = 2;
    lua["message_type_error"] = 3;
    lua["message_type_info"] = 4;
    
    // api.message_send_text(account_name, type, from, text)
    lua["api"] = lua.create_table();
    auto api = lua["api"];
    
    api["message_send_text"] = [&context](const std::string& account_name, int type, 
                                          const std::string& from, const std::string& text) {
        // TODO: Implement translation to new API
    };
}

void LegacyCompatShim::setup_command_api(sol::state& lua, PluginContext& context)
{
    // Legacy command functions
    
    // localize(account_name, text) -> string
    lua["localize"] = [&context](const std::string& account_name, const std::string& text) -> std::string {
        // TODO: Implement translation to new API
        return text;
    };
    
    // math_and(a, b) -> int (bitwise AND)
    lua["math_and"] = [](int a, int b) -> int {
        return a & b;
    };
}

} // namespace pvpgn::infra::scripting
