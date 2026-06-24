pvpgn_v3_add_test(test_domain_moderation
    SOURCES ip_ban_list_test.cpp
    DEPS    domain_moderation)

pvpgn_v3_add_test(test_domain_moderation_ip_ban_list_coverage
    SOURCES ip_ban_list_coverage_test.cpp
    DEPS    domain_moderation)
