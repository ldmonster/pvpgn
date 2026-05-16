#pragma once

#include <memory>
#include <string>
#include <functional>
#include <sol/sol.hpp>
#include "infra/scripting/plugin/i_plugin.hpp"
#include "infra/scripting/plugin/plugin_manifest.hpp"
#include "infra/scripting/plugin/capability.hpp"

namespace pvpgn::infra::scripting {

// Forward declarations
class PluginContext;

/// Lua plugin implementation
class LuaPlugin : public IPlugin {
public:
    explicit LuaPlugin(const PluginManifest& manifest, const std::string& plugin_dir);
    ~LuaPlugin() override;
    
    const PluginManifest& manifest() const override { return manifest_; }
    void init(PluginContext& context) override;
    void shutdown() override;
    CapabilitySet required_capabilities() const override { return required_caps_; }
    bool is_ready() const override { return ready_; }
    
    /// Get the Lua state
    sol::state& lua_state() { return lua_; }
    
private:
    PluginManifest manifest_;
    std::string plugin_dir_;
    sol::state lua_;
    CapabilitySet required_caps_;
    bool ready_ = false;
    
    void setup_sandbox();
    void load_entry_point();
};

/// Lua host managing multiple Lua plugins
class LuaHost {
public:
    LuaHost();
    ~LuaHost();
    
    /// Create a new Lua plugin from manifest
    std::unique_ptr<LuaPlugin> create_plugin(const PluginManifest& manifest, const std::string& plugin_dir);
    
    /// Setup bindings for a plugin context
    void setup_bindings(sol::state& lua, PluginContext& context, const CapabilitySet& caps);
    
private:
    void setup_account_api(sol::state& lua, PluginContext& context, const CapabilitySet& caps);
    void setup_chat_api(sol::state& lua, PluginContext& context, const CapabilitySet& caps);
    void setup_commands_api(sol::state& lua, PluginContext& context, const CapabilitySet& caps);
    void setup_events_api(sol::state& lua, PluginContext& context, const CapabilitySet& caps);
    void setup_store_api(sol::state& lua, PluginContext& context, const CapabilitySet& caps);
    void setup_http_api(sol::state& lua, PluginContext& context, const CapabilitySet& caps);
    void setup_moderation_api(sol::state& lua, PluginContext& context, const CapabilitySet& caps);
};

} // namespace pvpgn::infra::scripting
