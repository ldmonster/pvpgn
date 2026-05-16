#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace pvpgn::infra::scripting {

/// Thread-safe key-value store for plugin data
class PluginStore {
public:
    /// Set a value in the store for a plugin
    void set(std::string_view plugin_id, std::string_view key, std::string value);
    
    /// Get a value from the store for a plugin
    std::optional<std::string> get(std::string_view plugin_id, std::string_view key) const;
    
    /// Remove a key from the store for a plugin
    bool remove(std::string_view plugin_id, std::string_view key);
    
    /// Get all keys for a plugin
    std::vector<std::string> keys(std::string_view plugin_id) const;
    
    /// Clear all keys for a plugin
    void clear(std::string_view plugin_id);
    
    /// Get the singleton instance
    static PluginStore& instance();
    
private:
    PluginStore() = default;
    ~PluginStore() = default;
    
    // Prevent copying
    PluginStore(const PluginStore&) = delete;
    PluginStore& operator=(const PluginStore&) = delete;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data_;
};

} // namespace pvpgn::infra::scripting
