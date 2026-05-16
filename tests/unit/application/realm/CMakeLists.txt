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
