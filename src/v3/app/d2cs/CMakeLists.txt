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
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        protocol_d2cs
        domain_d2cs
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
        ${CMAKE_SOURCE_DIR}/src/v3/app/bnetd/include
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

# Install
install(TARGETS pvpgn_v3_d2cs
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
