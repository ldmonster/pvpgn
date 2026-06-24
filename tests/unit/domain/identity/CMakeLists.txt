pvpgn_v3_add_test(test_domain_identity_account
    SOURCES account_test.cpp
    DEPS    domain_identity)

pvpgn_v3_add_test(test_domain_identity_attribute_map
    SOURCES attribute_map_test.cpp
    DEPS    domain_identity)

pvpgn_v3_add_test(test_domain_identity_attribute_map_typed
    SOURCES attribute_map_typed_test.cpp
    DEPS    domain_identity)

pvpgn_v3_add_test(test_domain_identity_attribute_map_timestamps
    SOURCES attribute_map_timestamps_test.cpp
    DEPS    domain_identity)

pvpgn_v3_add_test(test_domain_identity_account_rotation
    SOURCES account_rotation_test.cpp
    DEPS    domain_identity)

pvpgn_v3_add_test(test_domain_identity_attribute_map_profile
    SOURCES attribute_map_profile_test.cpp
    DEPS    domain_identity)
