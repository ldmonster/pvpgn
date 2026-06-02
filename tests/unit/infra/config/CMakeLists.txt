pvpgn_v3_add_test(test_infra_config_server
    SOURCES server_config_test.cpp
    DEPS    infra_config)
# Plan 10: TOML parser no-panic property test.
pvpgn_v3_add_test(test_infra_config_toml_validator_property
    SOURCES toml_validator_property_test.cpp
    DEPS    infra_config)
pvpgn_v3_add_test(test_infra_config_legacy_prefs
    SOURCES legacy_prefs_test.cpp
    DEPS    infra_config)
pvpgn_v3_add_test(test_infra_config_config
    SOURCES test_config.cpp
    DEPS    infra_config)
pvpgn_v3_add_test(test_infra_config_d2cs_server
    SOURCES d2cs_server_config_test.cpp
    DEPS    infra_config)
pvpgn_v3_add_test(test_infra_config_d2dbs_server
    SOURCES d2dbs_server_config_test.cpp
    DEPS    infra_config)
pvpgn_v3_add_test(test_infra_config_d2cs_legacy_prefs
    SOURCES d2cs_legacy_prefs_test.cpp
    DEPS    infra_config)
pvpgn_v3_add_test(test_infra_config_d2dbs_legacy_prefs
    SOURCES d2dbs_legacy_prefs_test.cpp
    DEPS    infra_config)
pvpgn_v3_add_test(test_infra_config_conf_templates
    SOURCES conf_template_test.cpp
    DEPS    infra_config)
target_compile_definitions(test_infra_config_conf_templates PRIVATE
    PVPGN_CONF_SOURCE_DIR="${CMAKE_SOURCE_DIR}/conf")
pvpgn_v3_add_test(test_infra_config_prefs_dump
    SOURCES prefs_dump_test.cpp
    DEPS    infra_config)
