// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/scripting/lua/async_lua_db.hpp"
#include "core/logging.hpp"

namespace pvpgn::infra::scripting::lua {

AsyncLuaDb::AsyncLuaDb(FiberLuaScheduler& scheduler)
    : scheduler_(scheduler) {
}

void AsyncLuaDb::install(lua_State* L) {
    // Store the AsyncLuaDb pointer in the Lua registry
    lua_pushlightuserdata(L, this);
    lua_setfield(L, LUA_REGISTRYINDEX, "__pvpgn_async_lua_db");
    
    // Create the pvpgn table if it doesn't exist
    lua_getglobal(L, "pvpgn");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_setglobal(L, "pvpgn");
        lua_getglobal(L, "pvpgn");
    }
    
    // Register the db_query function
    lua_pushcfunction(L, lua_db_query);
    lua_setfield(L, -2, "db_query");
    
    lua_pop(L, 1);  // Pop the pvpgn table
}

int AsyncLuaDb::lua_db_query(lua_State* L) {
    // Get the AsyncLuaDb pointer from the registry
    lua_getfield(L, LUA_REGISTRYINDEX, "__pvpgn_async_lua_db");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return luaL_error(L, "AsyncLuaDb not installed");
    }
    
    AsyncLuaDb* db = static_cast<AsyncLuaDb*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    
    if (!db) {
        return luaL_error(L, "Invalid AsyncLuaDb pointer");
    }
    
    // Get the SQL string argument
    if (!lua_isstring(L, 1)) {
        return luaL_error(L, "db_query expects a string argument");
    }
    
    const char* sql = lua_tostring(L, 1);
    
    // Get the scheduler from the registry
    lua_getfield(L, LUA_REGISTRYINDEX, "__pvpgn_fiber_scheduler");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return luaL_error(L, "FiberLuaScheduler not installed");
    }
    
    FiberLuaScheduler* scheduler = static_cast<FiberLuaScheduler*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    
    if (!scheduler) {
        return luaL_error(L, "Invalid FiberLuaScheduler pointer");
    }
    
    // Register the async operation
    uint64_t op_id = scheduler->register_async_op(L);
    
    // Store op_id in the registry for later retrieval
    lua_pushinteger(L, static_cast<lua_Integer>(op_id));
    lua_setfield(L, LUA_REGISTRYINDEX, "__pvpgn_current_op_id");
    
    // Store the SQL query in the registry for the callback
    lua_pushstring(L, sql);
    lua_setfield(L, LUA_REGISTRYINDEX, "__pvpgn_current_sql");
    
    // Yield the Lua coroutine
    // The actual fiber suspension happens when yield_for_async is called
    return lua_yield(L, 0);
}

void AsyncLuaDb::on_query_complete(uint64_t op_id, core::Result<DbResultSet, core::Error> result) {
    // Get the scheduler to find the coroutine
    // Note: In a real implementation, we would need to store the coroutine reference
    // and push the results onto its stack before resuming
    
    if (result.has_value()) {
        const auto& rows = result.value();
        
        // In a real implementation, we would:
        // 1. Get the Lua coroutine from the scheduler
        // 2. Push the result table onto its stack
        // 3. Call scheduler_.resume_coroutine(op_id, nresults)
        
        PVPGN_LOG_DEBUG("AsyncLuaDb::on_query_complete: query {} returned {} rows", op_id, rows.size());
        
        // For now, just resume with 1 result (the table)
        scheduler_.resume_coroutine(op_id, 1);
    } else {
        const auto& error = result.error();
        PVPGN_LOG_ERROR("AsyncLuaDb::on_query_complete: query {} failed: {}", op_id, error.message());
        
        // Resume with 0 results on error
        scheduler_.resume_coroutine(op_id, 0);
    }
}

} // namespace pvpgn::infra::scripting::lua
