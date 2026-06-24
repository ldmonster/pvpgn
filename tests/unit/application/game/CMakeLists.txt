# SPDX-License-Identifier: GPL-2.0-or-later

pvpgn_v3_add_test(test_application_game_start
    SOURCES start_game_test.cpp
    DEPS    application_game
            infra_storage_repository)

pvpgn_v3_add_test(test_application_game_join
    SOURCES join_game_test.cpp
    DEPS    application_game
            infra_storage_repository)

pvpgn_v3_add_test(test_application_game_leave
    SOURCES leave_game_test.cpp
    DEPS    application_game
            infra_storage_repository)

pvpgn_v3_add_test(test_application_game_join_errors
    SOURCES join_game_errors_test.cpp
    DEPS    application_game
            infra_storage_repository)

pvpgn_v3_add_test(test_application_game_leave_errors
    SOURCES leave_game_errors_test.cpp
    DEPS    application_game
            infra_storage_repository)

pvpgn_v3_add_test(test_application_game_cancel
    SOURCES cancel_game_test.cpp
    DEPS    application_game
            infra_storage_repository)

pvpgn_v3_add_test(test_application_game_create_private
    SOURCES create_private_game_test.cpp
    DEPS    application_game
            infra_inmemory)

pvpgn_v3_add_test(test_application_game_create_private_errors
    SOURCES create_private_game_errors_test.cpp
    DEPS    application_game
            infra_inmemory)

pvpgn_v3_add_test(test_application_game_list_public
    SOURCES list_public_games_test.cpp
    DEPS    application_game
            infra_inmemory)

pvpgn_v3_add_test(test_application_game_report_result
    SOURCES report_game_result_test.cpp
    DEPS    application_game
            infra_inmemory)

pvpgn_v3_add_test(test_application_game_report_result_errors
    SOURCES report_game_result_errors_test.cpp
    DEPS    application_game
            infra_inmemory)
