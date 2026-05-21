pvpgn_v3_add_test(test_domain_shared_value_objects
    SOURCES value_objects_test.cpp
    DEPS    domain_shared)

pvpgn_v3_add_test(test_domain_shared_client_tag
    SOURCES client_tag_test.cpp
    DEPS    domain_shared)
