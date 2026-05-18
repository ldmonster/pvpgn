pvpgn_v3_add_test(test_protocol_irc_codec
    SOURCES codec_test.cpp
    DEPS    protocol_irc)

pvpgn_v3_add_test(test_protocol_irc_fsm
    SOURCES fsm_test.cpp
    DEPS    protocol_irc)

pvpgn_v3_add_test(test_protocol_irc_wire_types
    SOURCES wire_types_test.cpp
    DEPS    protocol_irc)
