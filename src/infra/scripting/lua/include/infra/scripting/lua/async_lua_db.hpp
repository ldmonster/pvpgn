#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later

/// @file async_lua_db.hpp
/// Async DB query wrapper for Lua scripts.
/// Allows Lua to call: local rows = pvpgn.db_query(sql)
/// which suspends the Lua coroutine + Boost.Fiber until results arrive.

#include "infra/scripting/lua/fiber_lua_scheduler.hpp"
#include "core/result.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace pvpgn::infra::scripting::lua {

using DbRow = std::unordered_map<std::string, std::string>;
using DbResultSet = std::vector<DbRow>;

/// Callback invoked when an async DB query completes.
/// The callback must push results onto the Lua stack and return nresults.
using DbQueryCallback = std::function<void(lua_State*, core::Result<DbResultSet, core::Error>)>;

/// AsyncLuaDb — provides Lua bindings for async database queries.
/// Integrates with FiberLuaScheduler to suspend fibers during DB I/O.
class AsyncLuaDb {
public:
    explicit AsyncLuaDb(FiberLuaScheduler& scheduler);

    /// Install pvpgn.db_query(sql) into the Lua state.
    void install(lua_State* L);

    /// Execute a query asynchronously and resume the coroutine when done.
    /// Called from the DB thread pool when a query completes.
    void on_query_complete(uint64_t op_id, core::Result<DbResultSet, core::Error> result);

private:
    FiberLuaScheduler& scheduler_;

    static int lua_db_query(lua_State* L);
};

} // namespace pvpgn::infra::scripting::lua
