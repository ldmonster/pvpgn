pvpgn_v3_add_test(test_domain_identity_account
    SOURCES account_test.cpp
    DEPS    domain_identity)

pvpgn_v3_add_test(test_domain_identity_attribute_map
    SOURCES attribute_map_test.cpp
    DEPS    domain_identity)
