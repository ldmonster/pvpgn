# SPDX-License-Identifier: GPL-2.0-or-later
# ---- app/d2dbs (D2DBS session handler) -------------------------------------
#
# `app_d2dbs` is a static library that bridges the D2DBS protocol FSM
# (`protocol_d2dbs`) to the domain use cases (`domain_d2dbs`).
#
# It contains:
#   - `D2DBSSessionHandler`  — translates D2DBSFsmCallbacks to use-case calls
#   - `ID2DBSSessionEgress`  — pure-virtual outbound port (header-only)

pvpgn_v3_add_library(app_d2dbs
    STATIC
    SOURCES
        src/d2dbs_session_handler.cpp
        # Sourced from legacy_d2dbs/src/
        src/send_packet_bridge.cpp
        src/send_echorequest_bridge.cpp
        src/d2dbs_prefs_bridge.cpp
        src/bridge_logger.cpp
        src/charlock_bridge.cpp
        src/d2ladder_bridge.cpp
        src/dbsdupecheck_bridge.cpp
        src/dbserver_bridge.cpp
        src/handle_signal_bridge.cpp
        src/dbspacket_bridge.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        protocol_d2dbs
        domain_d2dbs
        infra_config
)

# ---------------------------------------------------------------------------
# pvpgn_v3_d2dbs — standalone D2DBS server binary (composition root)
# ---------------------------------------------------------------------------
# Guard: only build when Boost is available (infra_net requires it).
if(NOT PVPGN_V3_WITH_BOOST)
    message(STATUS "pvpgn_v3_d2dbs: skipped (PVPGN_V3_WITH_BOOST=OFF)")
    return()
endif()

find_package(Threads REQUIRED)

# d2dbs_tcp_session.cpp is compiled here (not in app_d2dbs) because it
# #includes infra/net/tcp_session.hpp which requires Boost (infra_net).
add_executable(pvpgn_v3_d2dbs
    src/main.cpp
    src/d2dbs_tcp_session.cpp
)

pvpgn_v3_apply_flags(pvpgn_v3_d2dbs)

target_include_directories(pvpgn_v3_d2dbs
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        # Reuse the bnetd TcpListener (header-only wrapper around TcpAcceptor).
        ${CMAKE_SOURCE_DIR}/src/app/bnetd/include
)

target_link_libraries(pvpgn_v3_d2dbs
    PRIVATE
        app_d2dbs
        protocol_d2dbs
        domain_d2dbs
        infra_net
        core
        Boost::system
        Threads::Threads
)

install(TARGETS pvpgn_v3_d2dbs
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
