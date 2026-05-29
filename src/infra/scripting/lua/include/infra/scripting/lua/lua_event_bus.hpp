#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cstdint>

namespace pvpgn::infra::scripting {

/// Event bus for Lua plugins
class LuaEventBus {
public:
    using SubscriptionId = uint64_t;
    using Handler = std::function<void(const std::string& event, const std::string& json_data)>;
    
    /// Subscribe to an event
    /// Returns a subscription ID that can be used to unsubscribe
    SubscriptionId subscribe(std::string event_name, Handler handler);
    
    /// Unsubscribe from an event
    bool unsubscribe(SubscriptionId id);
    
    /// Emit an event to all subscribers
    void emit(std::string_view event_name, std::string_view json_data);
    
    /// Get the singleton instance
    static LuaEventBus& instance();
    
private:
    LuaEventBus() = default;
    ~LuaEventBus() = default;
    
    // Prevent copying
    LuaEventBus(const LuaEventBus&) = delete;
    LuaEventBus& operator=(const LuaEventBus&) = delete;
    
    mutable std::mutex mutex_;
    std::atomic<SubscriptionId> next_id_{1};
    std::unordered_map<SubscriptionId, std::pair<std::string, Handler>> subscriptions_;
};

} // namespace pvpgn::infra::scripting
