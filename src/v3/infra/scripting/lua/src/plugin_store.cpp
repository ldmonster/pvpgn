#include "infra/scripting/lua/plugin_store.hpp"

namespace pvpgn::infra::scripting {

void PluginStore::set(std::string_view plugin_id, std::string_view key, std::string value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    data_[std::string(plugin_id)][std::string(key)] = std::move(value);
}

std::optional<std::string> PluginStore::get(std::string_view plugin_id, std::string_view key) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto plugin_it = data_.find(std::string(plugin_id));
    if (plugin_it == data_.end()) {
        return std::nullopt;
    }
    
    auto key_it = plugin_it->second.find(std::string(key));
    if (key_it == plugin_it->second.end()) {
        return std::nullopt;
    }
    
    return key_it->second;
}

bool PluginStore::remove(std::string_view plugin_id, std::string_view key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto plugin_it = data_.find(std::string(plugin_id));
    if (plugin_it == data_.end()) {
        return false;
    }
    
    auto key_it = plugin_it->second.find(std::string(key));
    if (key_it == plugin_it->second.end()) {
        return false;
    }
    
    plugin_it->second.erase(key_it);
    return true;
}

std::vector<std::string> PluginStore::keys(std::string_view plugin_id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> result;
    auto plugin_it = data_.find(std::string(plugin_id));
    if (plugin_it != data_.end()) {
        for (const auto& [key, _] : plugin_it->second) {
            result.push_back(key);
        }
    }
    
    return result;
}

void PluginStore::clear(std::string_view plugin_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto plugin_it = data_.find(std::string(plugin_id));
    if (plugin_it != data_.end()) {
        plugin_it->second.clear();
    }
}

PluginStore& PluginStore::instance()
{
    static PluginStore instance;
    return instance;
}

} // namespace pvpgn::infra::scripting
