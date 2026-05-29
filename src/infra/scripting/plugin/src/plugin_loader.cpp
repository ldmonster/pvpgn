// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/scripting/plugin/plugin_loader.hpp"
#include <filesystem>

namespace fs = std::filesystem;
namespace pvpgn::infra::scripting {

PluginLoader::PluginLoader(const fs::path& plugins_dir)
    : plugins_dir_(plugins_dir)
{
    // Create plugins directory if it doesn't exist
    if (!fs::exists(plugins_dir_)) {
        fs::create_directories(plugins_dir_);
    }
}

PluginLoader::~PluginLoader()
{
    shutdown_all();
}

std::string PluginLoader::load_all()
{
    if (!fs::exists(plugins_dir_)) {
        return "Plugins directory does not exist: " + plugins_dir_.string();
    }
    
    std::string errors;
    
    for (const auto& entry : fs::directory_iterator(plugins_dir_)) {
        if (!entry.is_directory()) continue;
        
        std::string plugin_name = entry.path().filename().string();
        std::string error = load_plugin(plugin_name);
        
        if (!error.empty()) {
            if (!errors.empty()) errors += "\n";
            errors += "Failed to load plugin '" + plugin_name + "': " + error;
        }
    }
    
    return errors;
}

std::string PluginLoader::load_plugin(const std::string& plugin_name)
{
    fs::path plugin_dir = plugins_dir_ / plugin_name;
    
    if (!fs::exists(plugin_dir)) {
        return "Plugin directory not found: " + plugin_dir.string();
    }
    
    // For now, just return success without actually loading
    // This is a stub implementation since sol2 (Lua bindings) is not available
    return "";
}

std::string PluginLoader::unload_plugin(const std::string& plugin_name)
{
    auto it = plugins_.find(plugin_name);
    if (it == plugins_.end()) {
        return "Plugin not loaded: " + plugin_name;
    }
    
    plugins_.erase(it);
    return "";
}

std::string PluginLoader::reload_plugin(const std::string& plugin_name)
{
    std::string error = unload_plugin(plugin_name);
    if (!error.empty()) {
        return error;
    }
    
    return load_plugin(plugin_name);
}

IPlugin* PluginLoader::get_plugin(const std::string& plugin_name) const
{
    auto it = plugins_.find(plugin_name);
    if (it == plugins_.end()) {
        return nullptr;
    }
    
    return it->second.plugin.get();
}

std::vector<IPlugin*> PluginLoader::get_all_plugins() const
{
    std::vector<IPlugin*> result;
    for (const auto& [name, loaded] : plugins_) {
        result.push_back(loaded.plugin.get());
    }
    return result;
}

bool PluginLoader::is_loaded(const std::string& plugin_name) const
{
    return plugins_.find(plugin_name) != plugins_.end();
}

void PluginLoader::shutdown_all()
{
    plugins_.clear();
}

} // namespace pvpgn::infra::scripting
