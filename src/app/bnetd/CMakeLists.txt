# Defines the `bnetd` executable — the composition root that
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
#   protocol_irc    — IrcFsm + IrcTcpSession + IrcSessionFactory
#   infra_lua       — LuaRuntime + LuaConnectionContext

cmake_minimum_required(VERSION 3.20)

# Guard: only build when Boost is available (infra_net requires it).
if(NOT PVPGN_V3_WITH_BOOST)
    message(STATUS "pvpgn_v3_bnetd: skipped (PVPGN_V3_WITH_BOOST=OFF)")
    return()
endif()

find_package(Threads REQUIRED)

# The `LegacyBridge` glue (and its `app_bnetd_legacy_bridge` static library)
# were removed. `AsioEventLoop` (the one live TU it carried) now compiles
# directly into the `bnetd` exe.

# Renamed pvpgn_v3_bnetd → bnetd.
add_executable(bnetd
    src/main.cpp
    src/asio_event_loop.cpp
    src/session_manager.cpp
    src/bnet_connection_adapter.cpp
    src/lua_connection_context.cpp
    # main/ sub-modules (split from main.cpp for SRP)
    src/main/cli_args.cpp
    src/main/server_setup.cpp
    src/main/bnet_bnftp_dispatch.cpp
    src/main/wol_session.cpp
)

# Apply warning flags + C++20 standard
pvpgn_v3_apply_flags(bnetd)

# Public include path for app/bnetd headers
# PRIVATE src/ allows main/ sub-TUs to use #include "main/foo.hpp" paths.
target_include_directories(bnetd
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Core link dependencies
target_link_libraries(bnetd
    PRIVATE
        core
        domain_connection
        application_auth
        application_connection
        protocol_bnet
        protocol_file
        protocol_wol
        infra_net
        infra_inmemory
        infra_routing
        infra_crypto_session_hasher
        pvpgn_infra_file
        services_bnetd
        Boost::system
        Threads::Threads
)

# Optional: infra_config (TOML loading) — available when PVPGN_V3_WITH_TOMLPP=ON
if(TARGET infra_config)
    target_link_libraries(bnetd PRIVATE infra_config)
    target_compile_definitions(bnetd PRIVATE PVPGN_V3_BNETD_HAVE_CONFIG=1)
endif()

# Optional: infra_log (spdlog logger factory) — available when PVPGN_V3_WITH_SPDLOG=ON
if(TARGET infra_log)
    target_link_libraries(bnetd PRIVATE infra_log)
endif()

# PVPGN_VERSION (supplied by top-level CMakeLists.txt) for --version output.
target_compile_definitions(bnetd PRIVATE
    $<$<BOOL:${PVPGN_VERSION}>:PVPGN_VERSION="${PVPGN_VERSION}">)

# Smoke tests: --help and --version must succeed and emit output.
# These catch link-time + startup regressions in CI without needing a
# functional config or accept()-ing real clients.
if(BUILD_TESTING)
    add_test(NAME bnetd_help    COMMAND bnetd --help)
    add_test(NAME bnetd_version COMMAND bnetd --version)
    set_tests_properties(bnetd_help PROPERTIES
        PASS_REGULAR_EXPRESSION "Usage: bnetd")
    set_tests_properties(bnetd_version PROPERTIES
        PASS_REGULAR_EXPRESSION "bnetd ")
endif()

# Optional: protocol_irc — IrcFsm wired via IrcTcpSession + IrcSessionFactory
if(TARGET protocol_irc)
    target_link_libraries(bnetd PRIVATE protocol_irc)
    target_compile_definitions(bnetd PRIVATE PVPGN_V3_BNETD_HAVE_IRC=1)
endif()

# Optional: infra_lua — LuaRuntime + LuaConnectionContext
if(TARGET infra_lua)
    target_link_libraries(bnetd PRIVATE infra_lua)
endif()

# Persistence back-end targets (always link; compile-time guards handle availability)
if(TARGET pvpgn_infra_sqlite)
    target_link_libraries(bnetd PRIVATE pvpgn_infra_sqlite)
endif()
if(TARGET pvpgn_infra_mysql)
    target_link_libraries(bnetd PRIVATE pvpgn_infra_mysql)
endif()
if(TARGET pvpgn_infra_postgresql)
    target_link_libraries(bnetd PRIVATE pvpgn_infra_postgresql)
endif()

# Install
install(TARGETS bnetd
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
