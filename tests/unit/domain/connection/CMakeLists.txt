# SPDX-License-Identifier: GPL-2.0-or-later
# Unit tests for domain::connection::ConnectionFsm and RouteRegistry

pvpgn_v3_add_test(test_domain_connection_fsm
    SOURCES connection_fsm_test.cpp
    DEPS    domain_connection)

pvpgn_v3_add_test(test_domain_connection_route_registry
    SOURCES route_registry_test.cpp
    DEPS    domain_connection)
