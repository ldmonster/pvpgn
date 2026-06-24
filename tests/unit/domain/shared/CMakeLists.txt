pvpgn_v3_add_test(test_domain_shared_value_objects
    SOURCES value_objects_test.cpp
    DEPS
        domain_shared
        domain_chat
        domain_realm
        domain_social
        domain_gameplay)

pvpgn_v3_add_test(test_domain_shared_client_tag
    SOURCES client_tag_test.cpp
    DEPS    domain_shared)

# R341: backfill tests for domain value objects
pvpgn_v3_add_test(test_domain_shared_user_name
    SOURCES user_name_test.cpp
    DEPS    domain_shared)

pvpgn_v3_add_test(test_domain_shared_ip_address
    SOURCES ip_address_test.cpp
    DEPS    domain_shared)

pvpgn_v3_add_test(test_domain_shared_bn_hash
    SOURCES bn_hash_test.cpp
    DEPS    domain_shared)

# Net-new exhaustive branch coverage for shared value objects.
pvpgn_v3_add_test(test_domain_shared_user_name_exhaustive
    SOURCES user_name_exhaustive_test.cpp
    DEPS    domain_shared)

pvpgn_v3_add_test(test_domain_shared_locale_exhaustive
    SOURCES locale_exhaustive_test.cpp
    DEPS    domain_shared)

pvpgn_v3_add_test(test_domain_shared_client_tag_exhaustive
    SOURCES client_tag_exhaustive_test.cpp
    DEPS    domain_shared)

pvpgn_v3_add_test(test_domain_shared_ip_address_exhaustive
    SOURCES ip_address_exhaustive_test.cpp
    DEPS    domain_shared)

pvpgn_v3_add_test(test_domain_shared_account_id
    SOURCES account_id_test.cpp
    DEPS    domain_shared)

# R213a (plans/01-modern-cpp-baseline.md S3): every public header in
# src/v3/domain/shared/include/domain/shared/ must compile in isolation.
pvpgn_v3_add_header_selfcheck(test_domain_shared_headers_selfcontained
    INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src/domain/shared/include
    HEADERS
        domain/shared/ban.hpp
        domain/shared/bn_hash.hpp
        domain/shared/chat_message.hpp
        domain/shared/client_tag.hpp
        domain/shared/d2_character_class.hpp
        domain/shared/events.hpp
        domain/shared/ids.hpp
        domain/shared/ip_address.hpp
        domain/shared/locale.hpp
        domain/shared/match_report.hpp
        domain/shared/permission.hpp
        domain/shared/user_name.hpp
        domain/shared/ports/config_subscriber.hpp
        domain/shared/ports/event_loop.hpp
        domain/shared/ports/trace_sink.hpp
        domain/shared/ports/resolver.hpp
        domain/shared/ports/random_source.hpp
        domain/shared/ports/icon_provider.hpp
    DEPS domain_shared
)

# R253: selfcheck for new domain value-object headers.
pvpgn_v3_add_header_selfcheck(test_domain_chat_channel_name_selfcontained
    INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src/domain/chat/include
    HEADERS
        domain/chat/channel_name.hpp
    DEPS domain_chat
)

pvpgn_v3_add_header_selfcheck(test_domain_realm_realm_name_selfcontained
    INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src/domain/realm/include
    HEADERS
        domain/realm/realm_name.hpp
    DEPS domain_realm
)

pvpgn_v3_add_header_selfcheck(test_domain_social_clan_tag_selfcontained
    INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src/domain/social/include
    HEADERS
        domain/social/clan_tag.hpp
    DEPS domain_social
)

pvpgn_v3_add_header_selfcheck(test_domain_gameplay_game_name_selfcontained
    INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src/domain/gameplay/include
    HEADERS
        domain/gameplay/game_name.hpp
    DEPS domain_gameplay
)
