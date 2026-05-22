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

add_executable(pvpgn_v3_bnetd
    src/main.cpp
    src/session_manager.cpp
    src/asio_event_loop.cpp
    src/legacy_bridge.cpp
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
        Boost::system
        Threads::Threads
)

# Optional: infra_config (TOML loading) — available when PVPGN_V3_WITH_TOMLPP=ON
if(TARGET infra_config)
    target_link_libraries(pvpgn_v3_bnetd PRIVATE infra_config)
    target_compile_definitions(pvpgn_v3_bnetd PRIVATE PVPGN_V3_BNETD_HAVE_CONFIG=1)
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
# Add server_v3_hook.cpp to the bnetd_legacy static library when the v3
# build is active.  The hook is compiled with PVPGN_V3_BNETD_INTEGRATION=1
# (already set on bnetd_legacy by src/bnetd/CMakeLists.txt when the
# integration_legacy_bnetd_linked target exists) so the non-stub path is
# compiled in.  The v3 app/bnetd include directory is added so that the
# hook can #include "app/bnetd/legacy_bridge.hpp".
if(TARGET bnetd_legacy)
    target_sources(bnetd_legacy PRIVATE
        ${CMAKE_SOURCE_DIR}/src/bnetd/server_v3_hook.cpp
    )
    target_include_directories(bnetd_legacy PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
endif()
