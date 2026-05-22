# SPDX-License-Identifier: GPL-2.0-or-later
# Unit tests for domain::d2dbs use cases and in-memory repositories

pvpgn_v3_add_test(test_domain_d2dbs_use_cases
    SOURCES use_cases_test.cpp
    DEPS    domain_d2dbs)
