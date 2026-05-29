#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace pvpgn::infra::scripting {

/// Command structure for Lua command registry
struct LuaCommand {
    std::string name;
    std::string description;
    std::function<bool(std::string_view account, std::string_view args)> handler;
};

/// Registry for Lua commands
class LuaCommandRegistry {
public:
    /// Register a command
    bool register_command(LuaCommand cmd);
    
    /// Unregister a command
    bool unregister_command(std::string_view name);
    
    /// List all registered commands
    std::vector<LuaCommand> list_commands() const;
    
    /// Execute a command
    bool execute(std::string_view account, std::string_view command_line);
    
    /// Get the singleton instance
    static LuaCommandRegistry& instance();
    
private:
    LuaCommandRegistry() = default;
    ~LuaCommandRegistry() = default;
    
    // Prevent copying
    LuaCommandRegistry(const LuaCommandRegistry&) = delete;
    LuaCommandRegistry& operator=(const LuaCommandRegistry&) = delete;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, LuaCommand> commands_;
};

} // namespace pvpgn::infra::scripting
