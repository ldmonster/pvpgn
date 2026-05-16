#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later

/// @file fiber_lua_scheduler.hpp
/// Fiber-aware Lua coroutine scheduler
/// Bridges Boost.Fiber cooperative scheduling with Lua coroutine yields
/// so that async DB/network calls from Lua scripts suspend the fiber
/// rather than blocking the OS thread.

#include <boost/fiber/all.hpp>
#include <lua.hpp>
#include <functional>
#include <memory>
#include <unordered_map>
#include "core/result.hpp"

namespace pvpgn::infra::scripting::lua {

/// Represents a pending async operation initiated from Lua
struct LuaAsyncOp {
    lua_State* coro;                          ///< The Lua coroutine thread
    boost::fibers::promise<int> promise;      ///< Resolved with nresults when done
    boost::fibers::future<int> future;        ///< Awaited by the fiber
};

/// Callback type for async operations: called with (lua_State* coro, nresults)
using AsyncCompletionCb = std::function<void(lua_State*, int)>;

/// FiberLuaScheduler — manages the mapping between Lua coroutines and Boost.Fibers.
///
/// Usage pattern:
///   1. Lua script calls pvpgn.async_db_query(sql, callback)
///   2. The C binding calls FiberLuaScheduler::yield_for_async(L, op_id)
///   3. The current Boost.Fiber suspends (boost::this_fiber::yield())
///   4. When the async op completes, FiberLuaScheduler::resume_coroutine(op_id, nresults) is called
///   5. The fiber resumes, which resumes the Lua coroutine
class FiberLuaScheduler {
public:
    FiberLuaScheduler();
    ~FiberLuaScheduler();

    // Non-copyable, non-movable
    FiberLuaScheduler(const FiberLuaScheduler&) = delete;
    FiberLuaScheduler& operator=(const FiberLuaScheduler&) = delete;

    /// Register a new async operation for a Lua coroutine.
    /// Returns an op_id that can be used to resume later.
    /// Called from within a Boost.Fiber context.
    [[nodiscard]] uint64_t register_async_op(lua_State* coro);

    /// Suspend the current Boost.Fiber until the async op completes.
    /// Returns the number of results pushed onto the Lua stack.
    /// Must be called from within a Boost.Fiber.
    [[nodiscard]] int yield_for_async(uint64_t op_id);

    /// Resume a suspended Lua coroutine after async op completion.
    /// Safe to call from any thread/fiber.
    /// nresults: number of values to return to Lua
    void resume_coroutine(uint64_t op_id, int nresults);

    /// Cancel a pending async op (e.g., on session disconnect).
    void cancel_async_op(uint64_t op_id);

    /// Number of pending async operations
    [[nodiscard]] size_t pending_count() const noexcept;

private:
    struct OpEntry {
        lua_State* coro;
        boost::fibers::promise<int> promise;
        boost::fibers::future<int> future;
    };

    mutable boost::fibers::mutex mutex_;
    std::unordered_map<uint64_t, OpEntry> pending_ops_;
    uint64_t next_op_id_{1};
};

/// Lua C function: pvpgn.fiber_yield()
/// Yields the current Lua coroutine AND suspends the Boost.Fiber.
/// Used internally by async wrappers.
int lua_fiber_yield(lua_State* L);

/// Install fiber-aware coroutine hooks into a Lua state.
/// Registers pvpgn.fiber_yield and sets up the scheduler reference.
void install_fiber_lua_hooks(lua_State* L, FiberLuaScheduler& scheduler);

} // namespace pvpgn::infra::scripting::lua
