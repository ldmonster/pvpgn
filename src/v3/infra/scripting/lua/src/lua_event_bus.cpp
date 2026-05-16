#include "infra/scripting/lua/lua_event_bus.hpp"

namespace pvpgn::infra::scripting {

LuaEventBus::SubscriptionId LuaEventBus::subscribe(std::string event_name, Handler handler)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    SubscriptionId id = next_id_.fetch_add(1, std::memory_order_relaxed);
    subscriptions_[id] = {std::move(event_name), std::move(handler)};
    
    return id;
}

bool LuaEventBus::unsubscribe(SubscriptionId id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = subscriptions_.find(id);
    if (it == subscriptions_.end()) {
        return false;
    }
    
    subscriptions_.erase(it);
    return true;
}

void LuaEventBus::emit(std::string_view event_name, std::string_view json_data)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string event_str(event_name);
    std::string data_str(json_data);
    
    for (const auto& [id, subscription] : subscriptions_) {
        const auto& [sub_event, handler] = subscription;
        
        // Only call handlers for matching event names
        if (sub_event == event_str) {
            try {
                handler(event_str, data_str);
            } catch (...) {
                // Silently ignore handler exceptions to prevent cascading failures
            }
        }
    }
}

LuaEventBus& LuaEventBus::instance()
{
    static LuaEventBus instance;
    return instance;
}

} // namespace pvpgn::infra::scripting
