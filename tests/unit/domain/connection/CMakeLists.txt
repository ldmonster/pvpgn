# SPDX-License-Identifier: GPL-2.0-or-later
# Unit tests for domain::connection::ConnectionFsm and RouteRegistry

# The application::connection::ConnectionFsm is split (per the TU size cap)
# into one core file plus one file per state; the unit tests mirror that split,
# one *_test.cpp per production TU, all sharing connection_fsm_test_fixtures.hpp.
foreach(_fsm_state IN ITEMS
        fsm                 # core: dispatch / close / replies (connection_fsm.cpp)
        fsm_connecting
        fsm_authenticating
        fsm_loggedin
        fsm_inchannel
        fsm_ingame)
    pvpgn_v3_add_test(test_domain_connection_${_fsm_state}
        SOURCES connection_${_fsm_state}_test.cpp
        DEPS    domain_connection application_connection)
    if(TARGET test_domain_connection_${_fsm_state} AND NOT MSVC)
        target_compile_options(test_domain_connection_${_fsm_state}
            PRIVATE -Wno-error=null-dereference)
    endif()
endforeach()

# Extra branch-coverage cases for the InChannel handlers (game_type switch arms,
# empty-channel early return, out-of-order rejects) — shares the same fixtures.
pvpgn_v3_add_test(test_domain_connection_fsm_inchannel_branches
    SOURCES connection_fsm_inchannel_branches_test.cpp
    DEPS    domain_connection application_connection)
if(TARGET test_domain_connection_fsm_inchannel_branches AND NOT MSVC)
    target_compile_options(test_domain_connection_fsm_inchannel_branches
        PRIVATE -Wno-error=null-dereference)
endif()

# Injected chat-use-case paths of the InChannel handlers (real JoinChannel /
# PostMessage / LeaveChannel over in-memory repos).
pvpgn_v3_add_test(test_domain_connection_fsm_inchannel_usecase
    SOURCES connection_fsm_inchannel_usecase_test.cpp
    DEPS    domain_connection application_connection application_chat infra_inmemory)
if(TARGET test_domain_connection_fsm_inchannel_usecase AND NOT MSVC)
    target_compile_options(test_domain_connection_fsm_inchannel_usecase
        PRIVATE -Wno-error=null-dereference)
endif()

# Injected LoginUserNls (SRP) paths of the Authenticating handlers.
pvpgn_v3_add_test(test_domain_connection_fsm_authenticating_usecase
    SOURCES connection_fsm_authenticating_usecase_test.cpp
    DEPS    domain_connection application_connection application_auth
            infra_crypto_nls infra_crypto_nls_adapter)
if(TARGET test_domain_connection_fsm_authenticating_usecase AND NOT MSVC)
    target_compile_options(test_domain_connection_fsm_authenticating_usecase
        PRIVATE -Wno-error=null-dereference)
endif()

# Injected LoginUser (OLS) path of the Connecting handler (on_logon_request).
pvpgn_v3_add_test(test_domain_connection_fsm_connecting_usecase
    SOURCES connection_fsm_connecting_usecase_test.cpp
    DEPS    domain_connection application_connection application_auth
            infra_inmemory infra_crypto_nls infra_crypto_nls_adapter)
if(TARGET test_domain_connection_fsm_connecting_usecase AND NOT MSVC)
    target_compile_options(test_domain_connection_fsm_connecting_usecase
        PRIVATE -Wno-error=null-dereference)
endif()

# Dispatch paths of the InGame handlers (D2 char-select, WAR3 route token).
pvpgn_v3_add_test(test_domain_connection_fsm_ingame_dispatch
    SOURCES connection_fsm_ingame_dispatch_test.cpp
    DEPS    domain_connection application_connection)
if(TARGET test_domain_connection_fsm_ingame_dispatch AND NOT MSVC)
    target_compile_options(test_domain_connection_fsm_ingame_dispatch
        PRIVATE -Wno-error=null-dereference)
endif()

# Error / edge branches of the InChannel handlers: empty-text chat no-op,
# over-long / not-in-channel PostMessage EID_ERROR, parsed-ClientTag join,
# existing-member EID_SHOWUSER, leave-without-join, out-of-order leave.
pvpgn_v3_add_test(test_domain_connection_fsm_inchannel_error_branches
    SOURCES connection_fsm_inchannel_error_branches_test.cpp
    DEPS    domain_connection application_connection application_chat infra_inmemory)
if(TARGET test_domain_connection_fsm_inchannel_error_branches AND NOT MSVC)
    target_compile_options(test_domain_connection_fsm_inchannel_error_branches
        PRIVATE -Wno-error=null-dereference)
endif()

# Net-new InChannel SUCCESS-path coverage: decodes the SID_STARTADVEX /
# SID_GETADVLISTEX 4-byte reply bodies and full GameInfo per game_type arm, plus
# the EID_CHANNEL/EID_SHOWUSER/EID_JOIN join wire bodies and the EID_TALK chat
# echo (injected JoinChannel / PostMessage over in-memory repos).
pvpgn_v3_add_test(test_domain_connection_fsm_inchannel_success
    SOURCES connection_fsm_inchannel_success_test.cpp
    DEPS    domain_connection application_connection application_chat infra_inmemory)
if(TARGET test_domain_connection_fsm_inchannel_success AND NOT MSVC)
    target_compile_options(test_domain_connection_fsm_inchannel_success
        PRIVATE -Wno-error=null-dereference)
endif()

# Core dispatch-routing branches: PING/NULL in the remaining states, the
# LOGONRESPONSE2 / STARTADVEX3 aliases, unknown-packet ignore from each state,
# close() from Authenticating, and the post-reject Disconnecting drain.
pvpgn_v3_add_test(test_domain_connection_fsm_dispatch_branches
    SOURCES connection_fsm_dispatch_branches_test.cpp
    DEPS    domain_connection application_connection)
if(TARGET test_domain_connection_fsm_dispatch_branches AND NOT MSVC)
    target_compile_options(test_domain_connection_fsm_dispatch_branches
        PRIVATE -Wno-error=null-dereference)
endif()

# Connecting + Authenticating reject / fallback branches: AUTH_CHECK out of
# order, empty-username LOGON_REQUEST / ACCOUNTLOGON, injected-OLS invalid
# username, and the PROOF empty-username fallback.
pvpgn_v3_add_test(test_domain_connection_fsm_auth_reject_branches
    SOURCES connection_fsm_auth_reject_branches_test.cpp
    DEPS    domain_connection application_connection application_auth
            infra_inmemory infra_crypto_nls infra_crypto_nls_adapter)
if(TARGET test_domain_connection_fsm_auth_reject_branches AND NOT MSVC)
    target_compile_options(test_domain_connection_fsm_auth_reject_branches
        PRIVATE -Wno-error=null-dereference)
endif()

pvpgn_v3_add_test(test_domain_connection_route_registry
    SOURCES route_registry_test.cpp
    DEPS    domain_connection application_connection)
