# TODO: Fix character_persistence_test - needs protocol_d2save codec
# pvpgn_v3_add_test(application_realm_character_persistence_test
#     SOURCES character_persistence_test.cpp
#     DEPS application_realm)

pvpgn_v3_add_test(application_realm_character_lock_test
    SOURCES character_lock_test.cpp
    DEPS application_realm)

pvpgn_v3_add_test(application_realm_gs_queue_test
    SOURCES gs_queue_test.cpp
    DEPS application_realm)

pvpgn_v3_add_test(application_realm_realm_list_test
    SOURCES realm_list_test.cpp
    DEPS application_realm)

pvpgn_v3_add_test(application_realm_register_realm_test
    SOURCES register_realm_test.cpp
    DEPS application_realm)

pvpgn_v3_add_test(application_realm_heartbeat_realm_test
    SOURCES heartbeat_realm_test.cpp
    DEPS application_realm)

pvpgn_v3_add_test(application_realm_realm_auth_test
    SOURCES realm_auth_test.cpp
    DEPS application_realm)
