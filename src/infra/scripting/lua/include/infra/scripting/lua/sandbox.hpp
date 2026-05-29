#pragma once

#include <sol/sol.hpp>
#include "infra/scripting/plugin/capability.hpp"

namespace pvpgn::infra::scripting {

/// Lua sandbox configuration and setup
class LuaSandbox {
public:
    /// Setup sandbox restrictions for a Lua state
    /// Removes dangerous functions based on capabilities
    static void setup(sol::state& lua, const CapabilitySet& caps);
    
    /// Restrict os library
    static void restrict_os(sol::state& lua);
    
    /// Restrict io library
    static void restrict_io(sol::state& lua, bool allow_read, bool allow_write);
    
    /// Restrict debug library
    static void restrict_debug(sol::state& lua);
    
    /// Restrict loadfile/dofile/require
    static void restrict_loading(sol::state& lua);
    
    /// Set memory limit (in bytes)
    static void set_memory_limit(sol::state& lua, std::size_t limit);
    
    /// Set instruction count limit (for CPU throttling)
    static void set_instruction_limit(sol::state& lua, std::size_t limit);
};

} // namespace pvpgn::infra::scripting
