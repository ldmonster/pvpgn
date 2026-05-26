pvpgn_v3_add_test(test_application_anongame_lobby
    SOURCES lobby_test.cpp
    DEPS    application_anongame_lobby)

pvpgn_v3_add_test(test_application_anongame_lobby_game_type
    SOURCES game_type_test.cpp
    DEPS    application_anongame_lobby)
