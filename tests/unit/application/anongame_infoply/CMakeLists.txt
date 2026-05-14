pvpgn_v3_add_test(test_application_anongame_infoply
    SOURCES inforeply_builder_test.cpp
    DEPS    application_anongame_infoply infra_compression)

pvpgn_v3_add_test(test_application_anongame_infoply_type_composer
    SOURCES type_composer_test.cpp
    DEPS    application_anongame_infoply)

pvpgn_v3_add_test(test_application_anongame_infoply_tournament_decorator
    SOURCES tournament_decorator_test.cpp
    DEPS    application_anongame_infoply)
