# SPDX-License-Identifier: GPL-2.0-or-later
# Unit tests for domain::connection::ConnectionFsm

pvpgn_v3_add_test(test_domain_connection_fsm
    SOURCES connection_fsm_test.cpp
    DEPS    domain_connection)
