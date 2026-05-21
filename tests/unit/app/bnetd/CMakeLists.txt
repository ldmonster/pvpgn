# tests/unit/app/bnetd/CMakeLists.txt
#
# Unit tests for the app/bnetd composition-root wiring layer:
#   - BnftpEgressContext (IFileSessionContext adapter)
#   - FileSessionFactory (BnftpTcpSession factory callable)
#   - BnftpFsm state transitions via the IFileSessionContext interface
#   - IrcSessionFactory (IrcTcpSession factory callable)
#   - IrcFsm state transitions via a FakeIrcContext
#   - IRC codec encode/decode round-trip
#
# Requires Boost (infra_net depends on Boost.Asio).
# Skipped when PVPGN_V3_WITH_BOOST=OFF.

if(NOT PVPGN_V3_WITH_BOOST)
    message(STATUS "test_app_bnetd_bnftp_session: skipped (PVPGN_V3_WITH_BOOST=OFF)")
    message(STATUS "test_app_bnetd_irc_session: skipped (PVPGN_V3_WITH_BOOST=OFF)")
    return()
endif()

if(NOT TARGET infra_net)
    message(STATUS "test_app_bnetd_bnftp_session: skipped (infra_net not available)")
    message(STATUS "test_app_bnetd_irc_session: skipped (infra_net not available)")
    return()
endif()

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
        ${CMAKE_SOURCE_DIR}/src/v3/app/bnetd/include
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
        ${CMAKE_SOURCE_DIR}/src/v3/app/bnetd/include
)

endif()  # TARGET protocol_irc
