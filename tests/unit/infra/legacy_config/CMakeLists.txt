pvpgn_v3_add_test(test_infra_legacy_config_anongame_infos
    SOURCES anongame_infos_loader_test.cpp
    DEPS    infra_legacy_config)

pvpgn_v3_add_test(test_infra_legacy_config_anongame_maplists
    SOURCES anongame_maplists_loader_test.cpp
    DEPS    infra_legacy_config)

pvpgn_v3_add_test(test_infra_legacy_config_icon_req
    SOURCES icon_req_loader_test.cpp
    DEPS    infra_legacy_config)
