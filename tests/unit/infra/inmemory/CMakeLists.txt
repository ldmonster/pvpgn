# SPDX-License-Identifier: GPL-2.0-or-later

pvpgn_v3_add_test(test_infra_inmemory
    SOURCES
        in_memory_event_bus_test.cpp
        in_memory_unit_of_work_test.cpp
        in_memory_clan_repository_test.cpp
        in_memory_session_registry_test.cpp
        in_memory_ladder_repository_test.cpp
        in_memory_team_repository_test.cpp
        in_memory_mail_store_test.cpp
        in_memory_news_store_test.cpp
        in_memory_helpfile_source_test.cpp
        in_memory_icon_provider_test.cpp
        in_memory_random_source_test.cpp
        in_memory_session_token_issuer_test.cpp
        in_memory_message_broadcaster_test.cpp
        in_memory_permission_checker_test.cpp
        in_memory_channel_store_test.cpp
        in_memory_channel_repository_test.cpp
    DEPS
        infra_inmemory
        application_ports
)

if(TARGET infra_config)
    pvpgn_v3_add_test(test_infra_inmemory_config_subscriber
        SOURCES
            in_memory_config_subscriber_test.cpp
        DEPS
            infra_inmemory
            application_ports
            infra_config
    )
endif()
