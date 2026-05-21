pvpgn_v3_add_test(test_infra_config_server
    SOURCES server_config_test.cpp
    DEPS    infra_config)
pvpgn_v3_add_test(test_infra_config_legacy_prefs
    SOURCES legacy_prefs_test.cpp
    DEPS    infra_config)
pvpgn_v3_add_test(test_infra_config_config
    SOURCES test_config.cpp
    DEPS    infra_config)
