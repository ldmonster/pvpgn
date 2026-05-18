pvpgn_v3_add_test(test_protocol_d2gs_codec
    SOURCES codec_test.cpp
    DEPS    protocol_d2gs)

pvpgn_v3_add_test(test_protocol_d2gs_wire_types
    SOURCES wire_types_test.cpp
    DEPS    protocol_d2gs)

pvpgn_v3_add_test(test_protocol_d2gs_game_wire_types
    SOURCES game_wire_types_test.cpp
    DEPS    protocol_d2gs)
