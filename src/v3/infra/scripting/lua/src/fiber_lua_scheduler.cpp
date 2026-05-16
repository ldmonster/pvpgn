// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/scripting/lua/fiber_lua_scheduler.hpp"
#include "core/logging.hpp"
#include <stdexcept>

namespace pvpgn::infra::scripting::lua {

FiberLuaScheduler::FiberLuaScheduler() = default;

FiberLuaScheduler::~FiberLuaScheduler() = default;

uint64_t FiberLuaScheduler::register_async_op(lua_State* coro) {
    boost::fibers::lock_guard<boost::fibers::mutex> lock(mutex_);
    
    uint64_t op_id = next_op_id_++;
    
    OpEntry entry;
    entry.coro = coro;
    entry.promise = boost::fibers::promise<int>();
    entry.future = entry.promise.get_future();
    
    pending_ops_[op_id] = std::move(entry);
    
    return op_id;
}

int FiberLuaScheduler::yield_for_async(uint64_t op_id) {
    boost::fibers::future<int> future;
    
    {
        boost::fibers::lock_guard<boost::fibers::mutex> lock(mutex_);
        
        auto it = pending_ops_.find(op_id);
        if (it == pending_ops_.end()) {
            PVPGN_LOG_ERROR("FiberLuaScheduler::yield_for_async: op_id {} not found", op_id);
            return -1;
        }
        
        future = it->second.future;
    }
    
    // This call suspends the Boost.Fiber cooperatively
    int result = future.get();
    
    {
        boost::fibers::lock_guard<boost::fibers::mutex> lock(mutex_);
        pending_ops_.erase(op_id);
    }
    
    return result;
}

void FiberLuaScheduler::resume_coroutine(uint64_t op_id, int nresults) {
    boost::fibers::lock_guard<boost::fibers::mutex> lock(mutex_);
    
    auto it = pending_ops_.find(op_id);
    if (it == pending_ops_.end()) {
        PVPGN_LOG_WARN("FiberLuaScheduler::resume_coroutine: op_id {} not found", op_id);
        return;
    }
    
    // Set the result value, which wakes the suspended fiber
    it->second.promise.set_value(nresults);
}

void FiberLuaScheduler::cancel_async_op(uint64_t op_id) {
    boost::fibers::lock_guard<boost::fibers::mutex> lock(mutex_);
    
    auto it = pending_ops_.find(op_id);
    if (it == pending_ops_.end()) {
        PVPGN_LOG_WARN("FiberLuaScheduler::cancel_async_op: op_id {} not found", op_id);
        return;
    }
    
    // Set a special value (-1) to indicate cancellation
    it->second.promise.set_value(-1);
}

size_t FiberLuaScheduler::pending_count() const noexcept {
    boost::fibers::lock_guard<boost::fibers::mutex> lock(mutex_);
    return pending_ops_.size();
}

int lua_fiber_yield(lua_State* L) {
    // Get the scheduler from the Lua registry
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
    
    // Yield the Lua coroutine
    return lua_yield(L, 0);
}

void install_fiber_lua_hooks(lua_State* L, FiberLuaScheduler& scheduler) {
    // Store the scheduler pointer in the Lua registry
    lua_pushlightuserdata(L, &scheduler);
    lua_setfield(L, LUA_REGISTRYINDEX, "__pvpgn_fiber_scheduler");
    
    // Create the pvpgn table if it doesn't exist
    lua_getglobal(L, "pvpgn");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_setglobal(L, "pvpgn");
        lua_getglobal(L, "pvpgn");
    }
    
    // Register the fiber_yield function
    lua_pushcfunction(L, lua_fiber_yield);
    lua_setfield(L, -2, "fiber_yield");
    
    lua_pop(L, 1);  // Pop the pvpgn table
}

} // namespace pvpgn::infra::scripting::lua
