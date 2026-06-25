# Functional/Integration tests for v3 sub-tree

pvpgn_v3_add_test(test_functional_config_roundtrip
    SOURCES config_roundtrip_test.cpp
    DEPS    core infra_config)

pvpgn_v3_add_test(test_functional_plugin_lifecycle
    SOURCES plugin_lifecycle_test.cpp
    DEPS    core pvpgn_v3_plugin)

pvpgn_v3_add_test(test_functional_migrate_config
    SOURCES migrate_config_test.cpp
    DEPS    core)

pvpgn_v3_add_test(test_functional_migrate_accounts_ctime
    SOURCES migrate_accounts_ctime_test.cpp
    DEPS    core pvpgn_infra_file)
