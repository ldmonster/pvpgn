# SPDX-License-Identifier: GPL-2.0-or-later

pvpgn_v3_add_test(test_application_ladder_recompute
    SOURCES recompute_ladder_test.cpp
    DEPS    pvpgn_application_ladder
            pvpgn_domain_ladder
            pvpgn_domain_identity)

pvpgn_v3_add_test(test_application_ladder_get_entry
    SOURCES get_ladder_entry_test.cpp
    DEPS    pvpgn_application_ladder
            pvpgn_domain_ladder
            pvpgn_domain_identity)

pvpgn_v3_add_test(test_application_ladder_get_page
    SOURCES get_ladder_page_test.cpp
    DEPS    pvpgn_application_ladder
            pvpgn_domain_ladder
            pvpgn_domain_identity)
