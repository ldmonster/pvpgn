# SPDX-License-Identifier: GPL-2.0-or-later

pvpgn_v3_add_test(domain_realm_dupe_checker_test
    SOURCES dupe_checker_test.cpp
    DEPS domain_realm
)

pvpgn_v3_add_test(domain_realm_character_test
    SOURCES character_test.cpp
    DEPS domain_realm
)

pvpgn_v3_add_test(domain_realm_realm_test
    SOURCES realm_test.cpp
    DEPS domain_realm
)

pvpgn_v3_add_test(domain_realm_character_list_test
    SOURCES character_list_test.cpp
    DEPS domain_realm
)
