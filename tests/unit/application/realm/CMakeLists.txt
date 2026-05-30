# TODO: Fix character_persistence_test - needs protocol_d2save codec
# pvpgn_v3_add_test(application_realm_character_persistence_test
#     SOURCES character_persistence_test.cpp
#     DEPS application_realm)

# TODO(v3): character_lock_test constructs domain::realm::Character with
# the old 2-arg ctor; current ctor needs (id, stats, SystemTime).
# pvpgn_v3_add_test(application_realm_character_lock_test
#     SOURCES character_lock_test.cpp
#     DEPS application_realm)

pvpgn_v3_add_test(application_realm_gs_queue_test
    SOURCES gs_queue_test.cpp
    DEPS application_realm)

pvpgn_v3_add_test(application_realm_realm_list_test
    SOURCES realm_list_test.cpp
    DEPS application_realm)

# TODO(v3): register_realm test target depends on the (not-yet-present)
# application/ports/realm_repository.hpp port.
# pvpgn_v3_add_test(application_realm_register_realm_test
#     SOURCES register_realm_test.cpp
#     DEPS application_realm)

pvpgn_v3_add_test(application_realm_heartbeat_realm_test
    SOURCES heartbeat_realm_test.cpp
    DEPS application_realm)

pvpgn_v3_add_test(application_realm_realm_auth_test
    SOURCES realm_auth_test.cpp
    DEPS application_realm)

pvpgn_v3_add_test(application_realm_create_character_test
    SOURCES create_character_test.cpp
    DEPS application_realm)

pvpgn_v3_add_test(application_realm_delete_character_test
    SOURCES delete_character_test.cpp
    DEPS application_realm)

pvpgn_v3_add_test(application_realm_list_characters_test
    SOURCES list_characters_test.cpp
    DEPS application_realm)

pvpgn_v3_add_test(application_realm_load_character_test
    SOURCES load_character_test.cpp
    DEPS application_realm)

pvpgn_v3_add_test(application_realm_save_character_test
    SOURCES save_character_test.cpp
    DEPS application_realm)

pvpgn_v3_add_test(application_realm_join_game_server_test
    SOURCES join_game_server_test.cpp
    DEPS application_realm)

pvpgn_v3_add_test(application_realm_unregister_realm_test
    SOURCES unregister_realm_test.cpp
    DEPS application_realm)
