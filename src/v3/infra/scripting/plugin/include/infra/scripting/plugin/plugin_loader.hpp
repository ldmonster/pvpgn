#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include "i_plugin.hpp"

namespace pvpgn::infra::scripting {

/// Plugin loading and lifecycle management
class PluginLoader {
public:
    explicit PluginLoader(const std::filesystem::path& plugins_dir);
    ~PluginLoader();
    
    /// Discover and load all plugins from plugins directory
    /// Returns error message if any plugin fails to load
    std::string load_all();
    
    /// Load a single plugin by name
    /// Returns error message if loading fails
    std::string load_plugin(const std::string& plugin_name);
    
    /// Unload a plugin by name
    /// Returns error message if unloading fails
    std::string unload_plugin(const std::string& plugin_name);
    
    /// Reload a plugin (unload and reload)
    /// Returns error message if reloading fails
    std::string reload_plugin(const std::string& plugin_name);
    
    /// Get loaded plugin by name
    IPlugin* get_plugin(const std::string& plugin_name) const;
    
    /// Get all loaded plugins
    std::vector<IPlugin*> get_all_plugins() const;
    
    /// Check if plugin is loaded
    bool is_loaded(const std::string& plugin_name) const;
    
    /// Get plugin count
    std::size_t plugin_count() const { return plugins_.size(); }
    
    /// Shutdown all plugins
    void shutdown_all();
    
private:
    struct LoadedPlugin {
        std::unique_ptr<IPlugin> plugin;
        std::string plugin_dir;
        void* native_handle = nullptr; // for native plugins
    };
    
    std::filesystem::path plugins_dir_;
    std::unordered_map<std::string, LoadedPlugin> plugins_;
    
    std::string load_lua_plugin(const std::string& plugin_name, const std::filesystem::path& plugin_dir);
    std::string load_native_plugin(const std::string& plugin_name, const std::filesystem::path& plugin_dir);
};

} // namespace pvpgn::infra::scripting
