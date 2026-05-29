#include "infra/scripting/lua/sandbox.hpp"

namespace pvpgn::infra::scripting {

void LuaSandbox::setup(sol::state& lua, const CapabilitySet& caps)
{
    // Restrict os library
    restrict_os(lua);
    
    // Restrict io library based on capabilities
    bool allow_read = caps.has(Capability::FS_READ);
    bool allow_write = caps.has(Capability::FS_WRITE);
    restrict_io(lua, allow_read, allow_write);
    
    // Restrict debug library
    restrict_debug(lua);
    
    // Restrict loading functions
    restrict_loading(lua);
}

void LuaSandbox::restrict_os(sol::state& lua)
{
    // Remove dangerous os functions
    auto os = lua["os"];
    if (os.valid()) {
        os["execute"] = sol::nil;
        os["remove"] = sol::nil;
        os["rename"] = sol::nil;
        os["tmpname"] = sol::nil;
        os["exit"] = sol::nil;
    }
}

void LuaSandbox::restrict_io(sol::state& lua, bool allow_read, bool allow_write)
{
    auto io = lua["io"];
    if (io.valid()) {
        if (!allow_read && !allow_write) {
            // Disable all io functions
            io["open"] = sol::nil;
            io["input"] = sol::nil;
            io["output"] = sol::nil;
            io["lines"] = sol::nil;
            io["read"] = sol::nil;
            io["write"] = sol::nil;
        } else if (!allow_write) {
            // Allow read only
            io["write"] = sol::nil;
            io["output"] = sol::nil;
        } else if (!allow_read) {
            // Allow write only
            io["read"] = sol::nil;
            io["input"] = sol::nil;
            io["lines"] = sol::nil;
        }
    }
}

void LuaSandbox::restrict_debug(sol::state& lua)
{
    auto debug = lua["debug"];
    if (debug.valid()) {
        // Keep only traceback
        debug["getinfo"] = sol::nil;
        debug["getlocal"] = sol::nil;
        debug["getupvalue"] = sol::nil;
        debug["setlocal"] = sol::nil;
        debug["setupvalue"] = sol::nil;
        debug["sethook"] = sol::nil;
        debug["gethook"] = sol::nil;
    }
}

void LuaSandbox::restrict_loading(sol::state& lua)
{
    // Disable loadfile, dofile, require outside plugin directory
    lua["loadfile"] = sol::nil;
    lua["dofile"] = sol::nil;
    
    // TODO: Implement restricted require that only allows loading from plugin directory
    // lua["require"] = [plugin_dir](const std::string& module) { ... };
}

void LuaSandbox::set_memory_limit(sol::state& lua, std::size_t limit)
{
    // TODO: Implement memory limit using lua_setallocf
}

void LuaSandbox::set_instruction_limit(sol::state& lua, std::size_t limit)
{
    // TODO: Implement instruction count limit using lua_sethook
}

} // namespace pvpgn::infra::scripting
