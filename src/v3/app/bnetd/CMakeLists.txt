# src/v3/app/bnetd/CMakeLists.txt
#
# Defines the `pvpgn_v3_bnetd` executable — the v3 composition root that
# wires all protocol FSMs together with a Boost.Asio event loop.
#
# Dependencies
# ------------
#   protocol_bnet   — BnetFsm, BnetSessionContextImpl, codec
#   protocol_file   — BnftpFsm, IFileSessionContext
#   protocol_wol    — WolFsm, IWolSessionContext
#   infra_net       — IoRuntime, TcpAcceptor, TcpSession (Boost.Asio)
#   core            — Result<>, Error, ByteView
#   Boost::system   — Asio error_code
#   Threads         — std::thread
#
# Optional
#   infra_config    — TOML config loading (guarded by target existence)
#   protocol_irc    — IrcFsm + IrcTcpSession + IrcSessionFactory (R128)
#   infra_lua       — LuaRuntime + LuaConnectionContext (R141)

cmake_minimum_required(VERSION 3.20)

# Guard: only build when Boost is available (infra_net requires it).
if(NOT PVPGN_V3_WITH_BOOST)
    message(STATUS "pvpgn_v3_bnetd: skipped (PVPGN_V3_WITH_BOOST=OFF)")
    return()
endif()

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# R194: `app_bnetd_legacy_bridge` -- small static library carrying the
# `LegacyBridge` singleton + `AsioEventLoop` wrapper. Extracted so the
# legacy `bnetd` executable (which has `server_v3_hook.cpp` injected
# by the R193 strangler-fig wiring) can link against the symbols too;
# previously these TUs were compiled directly into `pvpgn_v3_bnetd`
# and unreachable from the legacy exe.
# ---------------------------------------------------------------------------
pvpgn_v3_add_library(app_bnetd_legacy_bridge STATIC
    SOURCES
        src/asio_event_loop.cpp
        src/legacy_bridge.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        Boost::system
)

# R194: both impl TUs gate their bodies on PVPGN_V3_BNETD_INTEGRATION.
# Set it PUBLIC so (a) the lib's own compilation sees the real bodies
# and (b) consumers see the matching class declaration (no ODR drift).
target_compile_definitions(app_bnetd_legacy_bridge PUBLIC
    PVPGN_V3_BNETD_INTEGRATION=1)

add_executable(pvpgn_v3_bnetd
    src/main.cpp
    src/session_manager.cpp
    src/bnet_connection_adapter.cpp
    src/lua_connection_context.cpp
)

# Apply v3 warning flags + C++20 standard
pvpgn_v3_apply_flags(pvpgn_v3_bnetd)

# Public include path for app/bnetd headers
target_include_directories(pvpgn_v3_bnetd
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Core link dependencies
target_link_libraries(pvpgn_v3_bnetd
    PRIVATE
        core
        domain_connection
        protocol_bnet
        protocol_file
        protocol_wol
        infra_net
        app_bnetd_legacy_bridge
        Boost::system
        Threads::Threads
)

# Optional: infra_config (TOML loading) — available when PVPGN_V3_WITH_TOMLPP=ON
if(TARGET infra_config)
    target_link_libraries(pvpgn_v3_bnetd PRIVATE infra_config)
    target_compile_definitions(pvpgn_v3_bnetd PRIVATE PVPGN_V3_BNETD_HAVE_CONFIG=1)
endif()

# Optional: infra_log (spdlog logger factory) — available when PVPGN_V3_WITH_SPDLOG=ON
if(TARGET infra_log)
    target_link_libraries(pvpgn_v3_bnetd PRIVATE infra_log)
endif()

# PVPGN_VERSION (supplied by top-level CMakeLists.txt) for --version output.
target_compile_definitions(pvpgn_v3_bnetd PRIVATE
    $<$<BOOL:${PVPGN_VERSION}>:PVPGN_VERSION="${PVPGN_VERSION}">)

# R167 smoke tests: --help and --version must succeed and emit output.
# These catch link-time + startup regressions in CI without needing a
# functional config or accept()-ing real clients.
if(BUILD_TESTING)
    add_test(NAME pvpgn_v3_bnetd_help    COMMAND pvpgn_v3_bnetd --help)
    add_test(NAME pvpgn_v3_bnetd_version COMMAND pvpgn_v3_bnetd --version)
    set_tests_properties(pvpgn_v3_bnetd_help PROPERTIES
        PASS_REGULAR_EXPRESSION "Usage: pvpgn_v3_bnetd")
    set_tests_properties(pvpgn_v3_bnetd_version PROPERTIES
        PASS_REGULAR_EXPRESSION "pvpgn_v3_bnetd ")
endif()

# Optional: protocol_irc — IrcFsm wired via IrcTcpSession + IrcSessionFactory (R128)
if(TARGET protocol_irc)
    target_link_libraries(pvpgn_v3_bnetd PRIVATE protocol_irc)
    target_compile_definitions(pvpgn_v3_bnetd PRIVATE PVPGN_V3_BNETD_HAVE_IRC=1)
endif()

# Optional: infra_lua — LuaRuntime + LuaConnectionContext (R141)
if(TARGET infra_lua)
    target_link_libraries(pvpgn_v3_bnetd PRIVATE infra_lua)
endif()

# Install
install(TARGETS pvpgn_v3_bnetd
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ---------------------------------------------------------------------------
# server_v3_hook: legacy bnetd hook that calls LegacyBridge::tick()
# ---------------------------------------------------------------------------
# MOVED to src/v3/integration/legacy_bnetd/CMakeLists.txt by R193.fix
# (Strategy B, two-pass `src/v3`). That CMakeLists is added LATE in the
# top-level configure so that bnetd_legacy already exists. The previous
# `if(TARGET bnetd_legacy)` placement here was UNREACHABLE because
# src/v3/app/bnetd/ is processed before src/bnetd/.
# See plans/r192-checklist.md + plans/r193-audit-checklist.md.
