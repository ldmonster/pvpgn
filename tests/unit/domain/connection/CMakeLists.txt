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

pvpgn_v3_add_test(test_domain_connection_route_registry
    SOURCES route_registry_test.cpp
    DEPS    domain_connection application_connection)
