pvpgn_v3_add_test(test_application_ports_clan_repository
    SOURCES clan_repository_test.cpp
    DEPS    domain_social application_ports)

# R213a: every public port header in
# src/v3/application/ports/include/application/ports/ must compile in
# isolation.
pvpgn_v3_add_header_selfcheck(test_application_ports_headers_selfcontained
    INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src/application/ports/include
    HEADERS
        application/ports/account_ban_repository.hpp
        application/ports/account_repository.hpp
        application/ports/audit_log.hpp
        application/ports/channel_repository.hpp
        application/ports/clan_repository.hpp
        application/ports/command_registry.hpp
        application/ports/config_subscriber.hpp
        application/ports/connection_handler.hpp
        application/ports/event_bus.hpp
        application/ports/friend_list_repository.hpp
        application/ports/game_repository.hpp
        application/ports/ip_ban_repository.hpp
        application/ports/ladder_repository.hpp
        application/ports/message_router.hpp
        application/ports/metrics_registry.hpp
        application/ports/password_hasher.hpp
        application/ports/permission_checker.hpp
        application/ports/realm_repository.hpp
        application/ports/session_registry.hpp
        application/ports/unit_of_work.hpp
        application/ports/unit_of_work_factory.hpp
    DEPS application_ports
         domain_shared
         domain_identity
         domain_chat
         domain_social
         domain_gameplay
         domain_moderation
         domain_realm
)
