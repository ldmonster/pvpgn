# src/v3/infra/lua/CMakeLists.txt
#
# Defines the `infra_lua` static library — a clean C++20 RAII wrapper
# around a Lua VM (`lua_State*`).
#
# When Lua is found (`PVPGN_HAVE_LUA` is set), the library links against
# the system Lua and compiles the full implementation.
#
# When Lua is NOT found, `infra_lua` still builds as a stub: all methods
# are no-ops (the `#ifdef PVPGN_HAVE_LUA` guards in lua_runtime.cpp handle
# this transparently).  Consumers can always link against `infra_lua`
# without worrying about whether Lua is available.

cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------
# Lua detection
# ---------------------------------------------------------------------------
# Try to find Lua 5.x.  We accept any version >= 5.1.
# The legacy bnetd code uses WITH_LUA; we use PVPGN_HAVE_LUA for the v3 tree
# so the two guards are independent.

find_package(Lua QUIET)

if(LUA_FOUND)
    message(STATUS "infra_lua: Lua ${LUA_VERSION_STRING} found — full build")
    set(PVPGN_HAVE_LUA_INTERNAL TRUE)
else()
    message(STATUS "infra_lua: Lua not found — building stub (all methods are no-ops)")
    set(PVPGN_HAVE_LUA_INTERNAL FALSE)
endif()

# ---------------------------------------------------------------------------
# infra_lua library target
# ---------------------------------------------------------------------------

pvpgn_v3_add_library(infra_lua
    STATIC
    SOURCES
        src/lua_runtime.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
)

# When Lua is available, define PVPGN_HAVE_LUA and link against Lua.
if(PVPGN_HAVE_LUA_INTERNAL)
    target_compile_definitions(infra_lua PUBLIC PVPGN_HAVE_LUA=1)
    target_include_directories(infra_lua PRIVATE ${LUA_INCLUDE_DIR})
    target_link_libraries(infra_lua PRIVATE ${LUA_LIBRARIES})
endif()

# Export a cache variable so parent scopes can query Lua availability.
set(PVPGN_V3_HAVE_LUA ${PVPGN_HAVE_LUA_INTERNAL}
    CACHE BOOL "Whether Lua was found for the v3 infra_lua library" FORCE)
