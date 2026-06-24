pvpgn_v3_add_test(test_application_social_add_friend
    SOURCES add_friend_test.cpp
    DEPS    application_social infra_inmemory)

pvpgn_v3_add_test(test_application_social_add_friend_outcomes
    SOURCES add_friend_outcomes_test.cpp
    DEPS    application_social infra_inmemory)

pvpgn_v3_add_test(test_application_social_remove_friend
    SOURCES remove_friend_test.cpp
    DEPS    application_social infra_inmemory)

pvpgn_v3_add_test(test_application_social_list_friends
    SOURCES list_friends_test.cpp
    DEPS    application_social infra_inmemory)

pvpgn_v3_add_test(test_application_social_list_friends_outcomes
    SOURCES list_friends_outcomes_test.cpp
    DEPS    application_social infra_inmemory)

pvpgn_v3_add_test(test_application_social_create_clan
    SOURCES create_clan_test.cpp
    DEPS    application_social infra_inmemory)

pvpgn_v3_add_test(test_application_social_disband_clan
    SOURCES disband_clan_test.cpp
    DEPS    application_social infra_inmemory)

pvpgn_v3_add_test(test_application_social_invite_to_clan
    SOURCES invite_to_clan_test.cpp
    DEPS    application_social infra_inmemory)

pvpgn_v3_add_test(test_application_social_kick_from_clan
    SOURCES kick_from_clan_test.cpp
    DEPS    application_social infra_inmemory)

pvpgn_v3_add_test(test_application_social_promote_clan_member
    SOURCES promote_clan_member_test.cpp
    DEPS    application_social infra_inmemory)

pvpgn_v3_add_test(test_application_social_promote_clan_member_outcomes
    SOURCES promote_clan_member_outcomes_test.cpp
    DEPS    application_social infra_inmemory)

pvpgn_v3_add_test(test_application_social_clan_full_outcomes
    SOURCES clan_full_outcomes_test.cpp
    DEPS    application_social infra_inmemory)

pvpgn_v3_add_test(test_application_social_clan_persistence_failure
    SOURCES clan_persistence_failure_test.cpp
    DEPS    application_social infra_inmemory)

pvpgn_v3_add_test(test_application_social_set_clan_motd
    SOURCES set_clan_motd_test.cpp
    DEPS    application_social infra_inmemory)

pvpgn_v3_add_test(test_application_social_join_clan
    SOURCES join_clan_test.cpp
    DEPS    application_social infra_inmemory)

pvpgn_v3_add_test(test_application_social_leave_clan
    SOURCES leave_clan_test.cpp
    DEPS    application_social infra_inmemory)

pvpgn_v3_add_test(test_application_social_create_team
    SOURCES create_team_test.cpp
    DEPS    application_social infra_inmemory)

pvpgn_v3_add_test(test_application_social_disband_team
    SOURCES disband_team_test.cpp
    DEPS    application_social infra_inmemory)
