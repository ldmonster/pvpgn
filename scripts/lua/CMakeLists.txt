# Install core bootstrap files for the Lua scripting layer.
# Feature plugins have been migrated to plugins/ (Plan 14).
# Source location: scripts/lua/ (moved from lua/ in Plan 04 Phase 4.1)
# Install destination: ${LOCALSTATEDIR}/lua (unchanged)

# Install top-level .lua files (main.lua, config.lua, handle_*.lua)
file(GLOB LUA_CORE_FILES "${PROJECT_SOURCE_DIR}/scripts/lua/*.lua")

INSTALL(FILES ${LUA_CORE_FILES} DESTINATION ${LOCALSTATEDIR}/lua)

# Install shared library directories (extend/ and include/)
INSTALL(DIRECTORY
    "${PROJECT_SOURCE_DIR}/scripts/lua/extend"
    "${PROJECT_SOURCE_DIR}/scripts/lua/include"
    DESTINATION ${LOCALSTATEDIR}/lua
)

# NOTE: Feature directories (quiz/, ghost/, antihack/, command/) have been
# migrated to plugins/ and are no longer installed from here.
