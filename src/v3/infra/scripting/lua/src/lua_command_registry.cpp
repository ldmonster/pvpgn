#include "infra/scripting/lua/lua_command_registry.hpp"
#include <sstream>

namespace pvpgn::infra::scripting {

bool LuaCommandRegistry::register_command(LuaCommand cmd)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = commands_.find(cmd.name);
    if (it != commands_.end()) {
        return false; // Command already exists
    }
    
    commands_[cmd.name] = std::move(cmd);
    return true;
}

bool LuaCommandRegistry::unregister_command(std::string_view name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = commands_.find(std::string(name));
    if (it == commands_.end()) {
        return false;
    }
    
    commands_.erase(it);
    return true;
}

std::vector<LuaCommand> LuaCommandRegistry::list_commands() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<LuaCommand> result;
    for (const auto& [name, cmd] : commands_) {
        result.push_back(cmd);
    }
    
    return result;
}

bool LuaCommandRegistry::execute(std::string_view account, std::string_view command_line)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Parse command name and arguments
    std::string cmd_str(command_line);
    std::istringstream iss(cmd_str);
    std::string cmd_name;
    
    if (!(iss >> cmd_name)) {
        return false; // Empty command
    }
    
    // Get remaining arguments
    std::string args;
    std::getline(iss, args);
    // Trim leading whitespace from args
    if (!args.empty() && args[0] == ' ') {
        args = args.substr(1);
    }
    
    auto it = commands_.find(cmd_name);
    if (it == commands_.end()) {
        return false; // Command not found
    }
    
    try {
        return it->second.handler(account, args);
    } catch (...) {
        return false; // Handler threw exception
    }
}

LuaCommandRegistry& LuaCommandRegistry::instance()
{
    static LuaCommandRegistry instance;
    return instance;
}

} // namespace pvpgn::infra::scripting
