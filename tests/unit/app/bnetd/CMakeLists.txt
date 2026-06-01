# tests/unit/app/bnetd/CMakeLists.txt
#
# Unit tests for the app/bnetd composition-root wiring layer:
#   - BnftpEgressContext (IFileSessionContext adapter)
#   - FileSessionFactory (BnftpTcpSession factory callable)
#   - BnftpFsm state transitions via the IFileSessionContext interface
#   - IrcSessionFactory (IrcTcpSession factory callable)
#   - IrcFsm state transitions via a FakeIrcContext
#   - IRC codec encode/decode round-trip
#   - AsioEventLoop lifecycle (run_for, post, stop, io_context)
#   - LegacyBridge tick / singleton lifecycle
#
# Requires Boost (infra_net depends on Boost.Asio).
# Skipped when PVPGN_V3_WITH_BOOST=OFF.

if(NOT PVPGN_V3_WITH_BOOST)
    message(STATUS "test_app_bnetd_bnftp_session: skipped (PVPGN_V3_WITH_BOOST=OFF)")
    message(STATUS "test_app_bnetd_irc_session: skipped (PVPGN_V3_WITH_BOOST=OFF)")
    message(STATUS "test_app_bnetd_asio_event_loop: skipped (PVPGN_V3_WITH_BOOST=OFF)")
    return()
endif()

if(NOT TARGET infra_net)
    message(STATUS "test_app_bnetd_bnftp_session: skipped (infra_net not available)")
    message(STATUS "test_app_bnetd_irc_session: skipped (infra_net not available)")
    message(STATUS "test_app_bnetd_asio_event_loop: skipped (infra_net not available)")
    return()
endif()

# ---------------------------------------------------------------------------
# AsioEventLoop + LegacyBridge tests (Round 138)
# ---------------------------------------------------------------------------

pvpgn_v3_add_test(test_app_bnetd_asio_event_loop
    SOURCES
        asio_event_loop_test.cpp
        ${CMAKE_SOURCE_DIR}/src/app/bnetd/src/asio_event_loop.cpp
        ${CMAKE_SOURCE_DIR}/src/app/bnetd/src/legacy_bridge.cpp
    DEPS
        infra_net
        Boost::system
        Threads::Threads
)

# PVPGN_V3_BNETD_INTEGRATION removed in Phase 3 (strangler finalization)

# Expose the app/bnetd include directory so the test can
# #include "app/bnetd/asio_event_loop.hpp" etc.
target_include_directories(test_app_bnetd_asio_event_loop
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/app/bnetd/include
)

pvpgn_v3_add_test(test_app_bnetd_bnftp_session
    SOURCES bnftp_session_test.cpp
    DEPS
        protocol_file
        protocol_wol
        infra_net
        application_ports
)

# The app/bnetd headers live in src/v3/app/bnetd/include/ which is not
# exported by any CMake target (pvpgn_v3_bnetd uses PRIVATE includes).
# Add the path directly so the test can #include "app/bnetd/...".
target_include_directories(test_app_bnetd_bnftp_session
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/app/bnetd/include
)

# ---------------------------------------------------------------------------
# IRC session wiring tests
# ---------------------------------------------------------------------------

if(NOT TARGET protocol_irc)
    message(STATUS "test_app_bnetd_irc_session: skipped (protocol_irc not available)")
else()

pvpgn_v3_add_test(test_app_bnetd_irc_session
    SOURCES irc_session_test.cpp
    DEPS
        protocol_irc
        infra_net
        application_ports
)

# The app/bnetd headers live in src/v3/app/bnetd/include/ which is not
# exported by any CMake target (pvpgn_v3_bnetd uses PRIVATE includes).
# Add the path directly so the test can #include "app/bnetd/...".
target_include_directories(test_app_bnetd_irc_session
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/app/bnetd/include
)

endif()  # TARGET protocol_irc

# ---------------------------------------------------------------------------
# BnetConnectionAdapter tests (Round 140)
# ---------------------------------------------------------------------------

pvpgn_v3_add_test(test_app_bnetd_connection_adapter
    SOURCES
        bnet_connection_adapter_test.cpp
        ${CMAKE_SOURCE_DIR}/src/app/bnetd/src/bnet_connection_adapter.cpp
        ${CMAKE_SOURCE_DIR}/src/application/connection/src/connection_fsm.cpp
    DEPS
        core
        domain_connection
        application_connection
        application_auth
        application_chat
        infra_net
)

# Expose the app/bnetd, domain/connection and application/* include directories.
target_include_directories(test_app_bnetd_connection_adapter
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/app/bnetd/include
        ${CMAKE_SOURCE_DIR}/src/domain/connection/include
        ${CMAKE_SOURCE_DIR}/src/application/connection/include
        ${CMAKE_SOURCE_DIR}/src/application/auth/include
)

# ---------------------------------------------------------------------------
# LuaConnectionContext tests (Round 141)
# ---------------------------------------------------------------------------

pvpgn_v3_add_test(test_app_bnetd_lua_connection_context
    SOURCES
        lua_connection_context_test.cpp
        ${CMAKE_SOURCE_DIR}/src/app/bnetd/src/lua_connection_context.cpp
    DEPS
        core
        domain_connection
        infra_lua
)

# Expose the app/bnetd, domain/connection, and infra/lua include directories.
target_include_directories(test_app_bnetd_lua_connection_context
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/app/bnetd/include
        ${CMAKE_SOURCE_DIR}/src/domain/connection/include
        ${CMAKE_SOURCE_DIR}/src/infra/lua/include
)
