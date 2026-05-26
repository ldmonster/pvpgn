pvpgn_v3_add_test(test_application_bnet_packet_pump_lifecycle
    SOURCES lifecycle_test.cpp
    DEPS    application_bnet_packet_pump)

pvpgn_v3_add_test(test_application_bnet_packet_pump_driver
    SOURCES driver_test.cpp
    DEPS    application_bnet_packet_pump)
