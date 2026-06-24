# SPDX-License-Identifier: GPL-2.0-or-later

pvpgn_v3_add_test(test_application_moderation_ip_ban
    SOURCES check_ip_ban_test.cpp
    DEPS    application_moderation)

pvpgn_v3_add_test(test_application_moderation_ip_ban_branches
    SOURCES check_ip_ban_branches_test.cpp
    DEPS    application_moderation)

pvpgn_v3_add_test(test_application_moderation_ban_account
    SOURCES ban_account_test.cpp
    DEPS    application_moderation)

pvpgn_v3_add_test(test_application_moderation_unban_account
    SOURCES unban_account_test.cpp
    DEPS    application_moderation)

pvpgn_v3_add_test(test_application_moderation_ban_ip
    SOURCES ban_ip_test.cpp
    DEPS    application_moderation)

pvpgn_v3_add_test(test_application_moderation_kick_connection
    SOURCES kick_connection_test.cpp
    DEPS    application_moderation domain_connection domain_identity)

pvpgn_v3_add_test(test_application_moderation_silence_user
    SOURCES silence_user_test.cpp
    DEPS    application_moderation)

pvpgn_v3_add_test(test_application_moderation_list_bans
    SOURCES list_bans_test.cpp
    DEPS    application_moderation)

pvpgn_v3_add_test(test_application_moderation_issue_warning
    SOURCES issue_warning_test.cpp
    DEPS    application_moderation)
