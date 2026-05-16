#include "infra/scripting/lua/lua_host.hpp"
#include "infra/scripting/lua/sandbox.hpp"
#include "infra/scripting/lua/plugin_store.hpp"
#include "infra/scripting/lua/lua_event_bus.hpp"
#include "infra/scripting/lua/lua_command_registry.hpp"
#include "infra/scripting/lua/simple_http_client.hpp"
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;
namespace pvpgn::infra::scripting {

LuaPlugin::LuaPlugin(const PluginManifest& manifest, const std::string& plugin_dir)
    : manifest_(manifest), plugin_dir_(plugin_dir)
{
    // Initialize Lua state
    lua_.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math);
    
    // Parse required capabilities
    required_caps_ = manifest_.parse_capabilities();
    
    // Setup sandbox
    setup_sandbox();
}

LuaPlugin::~LuaPlugin()
{
    shutdown();
}

void LuaPlugin::init(PluginContext& context)
{
    // Setup bindings
    LuaHost host;
    host.setup_bindings(lua_, context, required_caps_);
    
    // Load entry point
    load_entry_point();
    
    ready_ = true;
}

void LuaPlugin::shutdown()
{
    ready_ = false;
    // Lua state will be destroyed automatically
}

void LuaPlugin::setup_sandbox()
{
    // Setup sandbox restrictions
    LuaSandbox::setup(lua_, required_caps_);
}

void LuaPlugin::load_entry_point()
{
    fs::path entry_path = fs::path(plugin_dir_) / manifest_.entry;
    
    if (!fs::exists(entry_path)) {
        throw std::runtime_error("Entry point not found: " + entry_path.string());
    }
    
    // Load and execute the Lua file
    auto result = lua_.safe_script_file(entry_path.string());
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Failed to load entry point: " + std::string(err.what()));
    }
}

LuaHost::LuaHost()
{
}

LuaHost::~LuaHost()
{
}

std::unique_ptr<LuaPlugin> LuaHost::create_plugin(const PluginManifest& manifest, const std::string& plugin_dir)
{
    return std::make_unique<LuaPlugin>(manifest, plugin_dir);
}

void LuaHost::setup_bindings(sol::state& lua, PluginContext& context, const CapabilitySet& caps)
{
    // Create pvpgn namespace
    auto pvpgn = lua.create_named_table("pvpgn");
    
    // Setup API modules based on capabilities
    if (caps.has(Capability::DB_READ) || caps.has(Capability::DB_WRITE)) {
        setup_account_api(lua, context, caps);
    }
    
    if (caps.has(Capability::CHAT_SEND) || caps.has(Capability::CHAT_EMOTE)) {
        setup_chat_api(lua, context, caps);
    }
    
    if (caps.has(Capability::COMMANDS_REGISTER)) {
        setup_commands_api(lua, context, caps);
    }
    
    if (caps.has(Capability::EVENTS_SUBSCRIBE) || caps.has(Capability::EVENTS_PUBLISH)) {
        setup_events_api(lua, context, caps);
    }
    
    if (caps.has(Capability::STORE_READ) || caps.has(Capability::STORE_WRITE)) {
        setup_store_api(lua, context, caps);
    }
    
    if (caps.has(Capability::NET_HTTP)) {
        setup_http_api(lua, context, caps);
    }
    
    if (caps.has(Capability::MODERATION_BAN) || caps.has(Capability::MODERATION_KICK)) {
        setup_moderation_api(lua, context, caps);
    }
}

void LuaHost::setup_account_api(sol::state& lua, PluginContext& context, const CapabilitySet& caps)
{
    auto pvpgn = lua["pvpgn"];
    auto account = pvpgn.create_named_table("account");
    
    // pvpgn.account.find(name) -> returns account table or nil
    account["find"] = [](const std::string& name) -> sol::object {
        // Stub implementation - returns nil for now
        return sol::nil;
    };
    
    // pvpgn.account.create(name, password) -> returns bool
    account["create"] = [](const std::string& name, const std::string& password) -> bool {
        // Stub implementation
        return true;
    };
    
    // pvpgn.account.get_attr(name, key) -> returns string or nil
    account["get_attr"] = [](const std::string& name, const std::string& key) -> sol::object {
        // Stub implementation
        return sol::nil;
    };
    
    // pvpgn.account.set_attr(name, key, value) -> returns bool
    account["set_attr"] = [](const std::string& name, const std::string& key, const std::string& value) -> bool {
        // Stub implementation
        return true;
    };
    
    // pvpgn.account.is_online(name) -> returns bool
    account["is_online"] = [](const std::string& name) -> bool {
        // Stub implementation
        return false;
    };
    
    // pvpgn.account.get_online_count() -> returns integer
    account["get_online_count"] = []() -> int {
        // Stub implementation
        return 0;
    };
}

void LuaHost::setup_chat_api(sol::state& lua, PluginContext& context, const CapabilitySet& caps)
{
    auto pvpgn = lua["pvpgn"];
    auto chat = pvpgn.create_named_table("chat");
    
    // pvpgn.chat.send_message(channel, message) -> returns bool
    chat["send_message"] = [](const std::string& channel, const std::string& message) -> bool {
        // Stub implementation
        return true;
    };
    
    // pvpgn.chat.send_whisper(from, to, message) -> returns bool
    chat["send_whisper"] = [](const std::string& from, const std::string& to, const std::string& message) -> bool {
        // Stub implementation
        return true;
    };
    
    // pvpgn.chat.get_channel_users(channel) -> returns table of names
    chat["get_channel_users"] = [&lua](const std::string& channel) -> sol::table {
        auto result = lua.create_table();
        // Stub implementation - return empty table
        return result;
    };
    
    // pvpgn.chat.get_channels() -> returns table of channel names
    chat["get_channels"] = [&lua]() -> sol::table {
        auto result = lua.create_table();
        // Stub implementation - return empty table
        return result;
    };
    
    // pvpgn.chat.join_channel(account, channel) -> returns bool
    chat["join_channel"] = [](const std::string& account, const std::string& channel) -> bool {
        // Stub implementation
        return true;
    };
    
    // pvpgn.chat.kick_user(channel, account, reason) -> returns bool
    chat["kick_user"] = [](const std::string& channel, const std::string& account, const std::string& reason) -> bool {
        // Stub implementation
        return true;
    };
}

void LuaHost::setup_commands_api(sol::state& lua, PluginContext& context, const CapabilitySet& caps)
{
    auto pvpgn = lua["pvpgn"];
    auto commands = pvpgn.create_named_table("commands");
    
    // pvpgn.commands.register(name, handler, description) -> returns bool
    commands["register"] = [](const std::string& name, sol::function handler, const std::string& description) -> bool {
        LuaCommand cmd;
        cmd.name = name;
        cmd.description = description;
        cmd.handler = [handler](std::string_view account, std::string_view args) -> bool {
            try {
                auto result = handler(std::string(account), std::string(args));
                if (result.valid()) {
                    return result.get_or(false);
                }
            } catch (...) {
                return false;
            }
            return false;
        };
        
        return LuaCommandRegistry::instance().register_command(std::move(cmd));
    };
    
    // pvpgn.commands.unregister(name) -> returns bool
    commands["unregister"] = [](const std::string& name) -> bool {
        return LuaCommandRegistry::instance().unregister_command(name);
    };
    
    // pvpgn.commands.list() -> returns table of {name, description}
    commands["list"] = [&lua]() -> sol::table {
        auto result = lua.create_table();
        auto cmds = LuaCommandRegistry::instance().list_commands();
        
        int idx = 1;
        for (const auto& cmd : cmds) {
            auto cmd_table = lua.create_table();
            cmd_table["name"] = cmd.name;
            cmd_table["description"] = cmd.description;
            result[idx++] = cmd_table;
        }
        
        return result;
    };
    
    // pvpgn.commands.execute(account, command_line) -> returns bool
    commands["execute"] = [](const std::string& account, const std::string& command_line) -> bool {
        return LuaCommandRegistry::instance().execute(account, command_line);
    };
}

void LuaHost::setup_events_api(sol::state& lua, PluginContext& context, const CapabilitySet& caps)
{
    auto pvpgn = lua["pvpgn"];
    auto events = pvpgn.create_named_table("events");
    
    // pvpgn.events.subscribe(event_name, handler) -> returns subscription_id
    events["subscribe"] = [&lua](const std::string& event_name, sol::function handler) -> uint64_t {
        auto subscription_id = LuaEventBus::instance().subscribe(
            event_name,
            [handler, &lua](const std::string& event, const std::string& json_data) {
                try {
                    handler(event, json_data);
                } catch (...) {
                    // Silently ignore handler exceptions
                }
            }
        );
        return subscription_id;
    };
    
    // pvpgn.events.unsubscribe(subscription_id) -> returns bool
    events["unsubscribe"] = [](uint64_t subscription_id) -> bool {
        return LuaEventBus::instance().unsubscribe(subscription_id);
    };
    
    // pvpgn.events.emit(event_name, data) -> returns bool
    events["emit"] = [](const std::string& event_name, sol::object data) -> bool {
        // Convert data to JSON string
        std::string json_data = "{}";
        if (data.is<sol::table>()) {
            // Simple table to JSON conversion
            sol::table t = data.as<sol::table>();
            std::ostringstream oss;
            oss << "{";
            bool first = true;
            for (const auto& [key, value] : t) {
                if (!first) oss << ",";
                oss << "\"" << key.as<std::string>() << "\":";
                if (value.is<std::string>()) {
                    oss << "\"" << value.as<std::string>() << "\"";
                } else if (value.is<int>()) {
                    oss << value.as<int>();
                } else if (value.is<double>()) {
                    oss << value.as<double>();
                } else if (value.is<bool>()) {
                    oss << (value.as<bool>() ? "true" : "false");
                } else {
                    oss << "null";
                }
                first = false;
            }
            oss << "}";
            json_data = oss.str();
        } else if (data.is<std::string>()) {
            json_data = data.as<std::string>();
        }
        
        LuaEventBus::instance().emit(event_name, json_data);
        return true;
    };
}

void LuaHost::setup_store_api(sol::state& lua, PluginContext& context, const CapabilitySet& caps)
{
    auto pvpgn = lua["pvpgn"];
    auto store = pvpgn.create_named_table("store");
    
    // Get plugin ID from manifest
    std::string plugin_id = context.manifest().id;
    
    // pvpgn.store.get(key) -> returns string or nil
    store["get"] = [plugin_id](const std::string& key) -> sol::object {
        auto value = PluginStore::instance().get(plugin_id, key);
        if (value) {
            return sol::object(sol::in_place_type<std::string>, *value);
        }
        return sol::nil;
    };
    
    // pvpgn.store.set(key, value) -> returns bool
    store["set"] = [plugin_id](const std::string& key, const std::string& value) -> bool {
        PluginStore::instance().set(plugin_id, key, value);
        return true;
    };
    
    // pvpgn.store.delete(key) -> returns bool
    store["delete"] = [plugin_id](const std::string& key) -> bool {
        return PluginStore::instance().remove(plugin_id, key);
    };
    
    // pvpgn.store.keys() -> returns table of keys
    store["keys"] = [&lua, plugin_id]() -> sol::table {
        auto result = lua.create_table();
        auto keys = PluginStore::instance().keys(plugin_id);
        
        int idx = 1;
        for (const auto& key : keys) {
            result[idx++] = key;
        }
        
        return result;
    };
    
    // pvpgn.store.clear() -> clears all keys for this plugin
    store["clear"] = [plugin_id]() -> bool {
        PluginStore::instance().clear(plugin_id);
        return true;
    };
}

void LuaHost::setup_http_api(sol::state& lua, PluginContext& context, const CapabilitySet& caps)
{
    auto pvpgn = lua["pvpgn"];
    auto http = pvpgn.create_named_table("http");
    
    // pvpgn.http.get(url) -> returns {status, body}
    http["get"] = [&lua](const std::string& url) -> sol::table {
        auto result = lua.create_table();
        
        auto response = SimpleHttpClient::get(url);
        if (response.has_value()) {
            result["status"] = response->status_code;
            result["body"] = response->body;
            result["content_type"] = response->content_type;
        } else {
            result["status"] = 0;
            result["body"] = "";
            result["content_type"] = "";
        }
        
        return result;
    };
    
    // pvpgn.http.post(url, body, content_type) -> returns {status, body}
    http["post"] = [&lua](const std::string& url, const std::string& body, 
                          const std::string& content_type = "application/json") -> sol::table {
        auto result = lua.create_table();
        
        auto response = SimpleHttpClient::post(url, body, content_type);
        if (response.has_value()) {
            result["status"] = response->status_code;
            result["body"] = response->body;
            result["content_type"] = response->content_type;
        } else {
            result["status"] = 0;
            result["body"] = "";
            result["content_type"] = "";
        }
        
        return result;
    };
}

void LuaHost::setup_moderation_api(sol::state& lua, PluginContext& context, const CapabilitySet& caps)
{
    auto pvpgn = lua["pvpgn"];
    auto moderation = pvpgn.create_named_table("moderation");
    
    // pvpgn.moderation.ban_account(name, reason, duration_seconds) -> returns bool
    moderation["ban_account"] = [](const std::string& name, const std::string& reason, int duration_seconds) -> bool {
        // Stub implementation
        return true;
    };
    
    // pvpgn.moderation.unban_account(name) -> returns bool
    moderation["unban_account"] = [](const std::string& name) -> bool {
        // Stub implementation
        return true;
    };
    
    // pvpgn.moderation.ban_ip(ip, reason, duration_seconds) -> returns bool
    moderation["ban_ip"] = [](const std::string& ip, const std::string& reason, int duration_seconds) -> bool {
        // Stub implementation
        return true;
    };
    
    // pvpgn.moderation.is_banned(name) -> returns bool
    moderation["is_banned"] = [](const std::string& name) -> bool {
        // Stub implementation
        return false;
    };
    
    // pvpgn.moderation.get_ban_info(name) -> returns table or nil
    moderation["get_ban_info"] = [&lua](const std::string& name) -> sol::object {
        // Stub implementation - return nil
        return sol::nil;
    };
}

} // namespace pvpgn::infra::scripting
