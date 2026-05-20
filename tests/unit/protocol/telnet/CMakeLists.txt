pvpgn_v3_add_test(test_protocol_telnet_codec
    SOURCES codec_test.cpp
    DEPS    protocol_telnet)

pvpgn_v3_add_test(test_protocol_telnet_admin_fsm
    SOURCES admin_fsm_test.cpp
    DEPS    protocol_telnet application_ports)
