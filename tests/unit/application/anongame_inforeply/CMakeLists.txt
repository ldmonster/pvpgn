pvpgn_v3_add_test(test_application_anongame_inforeply
    SOURCES inforeply_builder_test.cpp
    DEPS    application_anongame_inforeply infra_compression)

pvpgn_v3_add_test(test_application_anongame_inforeply_type_composer
    SOURCES type_composer_test.cpp
    DEPS    application_anongame_inforeply)

pvpgn_v3_add_test(test_application_anongame_inforeply_tournament_decorator
    SOURCES tournament_decorator_test.cpp
    DEPS    application_anongame_inforeply)
