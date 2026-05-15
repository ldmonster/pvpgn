# SPDX-License-Identifier: GPL-2.0-or-later

pvpgn_v3_add_test(test_application_game_start
    SOURCES start_game_test.cpp
    DEPS    application_game
            infra_storage)

pvpgn_v3_add_test(test_application_game_join
    SOURCES join_game_test.cpp
    DEPS    application_game
            infra_storage)

pvpgn_v3_add_test(test_application_game_leave
    SOURCES leave_game_test.cpp
    DEPS    application_game
            infra_storage)
