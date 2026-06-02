# SPDX-License-Identifier: GPL-2.0-or-later
# ---- app/d2cs (D2CS session handler + composition root — Phase 4) ----------
#
# `app_d2cs` is a static library that bridges the D2CS protocol FSM
# (`protocol_d2cs`) to the domain use cases (`domain_d2cs`).
#
# It contains:
#   - `D2CSSessionHandler`  — translates D2CSFsmCallbacks to use-case calls
#   - `ID2CSSessionEgress`  — pure-virtual outbound port (header-only)
#   - `D2CSTcpSession`      — per-connection object (TCP + FSM + handler)
#
# `pvpgn_v3_d2cs` is the standalone D2CS server binary (composition root).
#
# Phase 4 Step 3 — R146 (library)
# Phase 4 Step 4 — R147 (composition root binary)

pvpgn_v3_add_library(app_d2cs
    STATIC
    SOURCES
        src/d2cs_session_handler.cpp
        # MIGRATED from legacy_d2cs/src/ (Plan 04 strangler)
        src/send_packet_bridge.cpp
        src/send_loginreply_bridge.cpp
        src/send_createcharreply_bridge.cpp
        src/send_charloginreply_bridge.cpp
        src/send_creategamereply_bridge.cpp
        src/send_joingamereply_bridge.cpp
        src/send_init_bnetd_bridge.cpp
        src/send_authreply_bnetd_bridge.cpp
        src/send_gameinforeply_bnetd_bridge.cpp
        src/send_handle_d2gs_bridges.cpp
        src/send_simple_replies.cpp
        src/send_listreply_bridges.cpp
        src/send_ladderreply_bridge.cpp
        src/send_charlistreply_bridge.cpp
        src/send_outbound_obs_bridges.cpp
        src/d2cs_prefs_bridge.cpp
        src/bridge_logger.cpp
        src/d2ladder_bridge.cpp
        src/s2s_bridge.cpp
        src/handle_signal_bridge.cpp
        src/server_bridge.cpp
        src/handle_init_bridge.cpp
        src/conn_bridge.cpp
        src/handle_d2cs_packet_bridge.cpp
        src/handle_d2gs_packet_bridge.cpp
        src/handle_bnetd_packet_bridge.cpp
        src/gamequeue_bridge.cpp
        src/serverqueue_bridge.cpp
        src/d2gs_bridge.cpp
        src/game_bridge.cpp
        src/net_bridge.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        protocol_d2cs
        protocol_d2gs
        domain_d2cs
        infra_config
)

# ---------------------------------------------------------------------------
# pvpgn_v3_d2cs — standalone D2CS server binary
# ---------------------------------------------------------------------------
# Guard: only build when Boost is available (infra_net requires it).

if(NOT PVPGN_V3_WITH_BOOST)
    message(STATUS "pvpgn_v3_d2cs: skipped (PVPGN_V3_WITH_BOOST=OFF)")
    return()
endif()

find_package(Threads REQUIRED)

# d2cs_tcp_session.cpp is compiled here (not in app_d2cs) because it
# #includes infra/net/tcp_session.hpp which requires Boost (infra_net).
add_executable(pvpgn_v3_d2cs
    src/main.cpp
    src/d2cs_tcp_session.cpp
)

# Apply v3 warning flags + C++20 standard
pvpgn_v3_apply_flags(pvpgn_v3_d2cs)

# Public include path for app/d2cs headers
target_include_directories(pvpgn_v3_d2cs
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        # Reuse bnetd TcpListener (header-only wrapper around TcpAcceptor)
        ${CMAKE_SOURCE_DIR}/src/app/bnetd/include
)

# Core link dependencies
target_link_libraries(pvpgn_v3_d2cs
    PRIVATE
        app_d2cs
        protocol_d2cs
        domain_d2cs
        infra_net
        core
        Boost::system
        Threads::Threads
)

# Optional: infra_config (TOML loading) — available when PVPGN_V3_WITH_TOMLPP=ON
if(TARGET infra_config)
    target_link_libraries(pvpgn_v3_d2cs PRIVATE infra_config)
    target_compile_definitions(pvpgn_v3_d2cs PRIVATE PVPGN_V3_D2CS_HAVE_CONFIG=1)
endif()

# Optional: infra_log (spdlog logger factory) — available when PVPGN_V3_WITH_SPDLOG=ON
if(TARGET infra_log)
    target_link_libraries(pvpgn_v3_d2cs PRIVATE infra_log)
endif()

# Install
install(TARGETS pvpgn_v3_d2cs
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# PVPGN_VERSION (from top-level CMakeLists.txt) for --version output.
target_compile_definitions(pvpgn_v3_d2cs PRIVATE
    $<$<BOOL:${PVPGN_VERSION}>:PVPGN_VERSION="${PVPGN_VERSION}">)

# R167 smoke tests.
if(BUILD_TESTING)
    add_test(NAME pvpgn_v3_d2cs_help    COMMAND pvpgn_v3_d2cs --help)
    add_test(NAME pvpgn_v3_d2cs_version COMMAND pvpgn_v3_d2cs --version)
    set_tests_properties(pvpgn_v3_d2cs_help PROPERTIES
        PASS_REGULAR_EXPRESSION "Usage: pvpgn_v3_d2cs")
    set_tests_properties(pvpgn_v3_d2cs_version PROPERTIES
        PASS_REGULAR_EXPRESSION "pvpgn_v3_d2cs ")
endif()
