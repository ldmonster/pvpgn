pvpgn_v3_add_test(test_protocol_wolgameres_codec
    SOURCES codec_test.cpp
    DEPS    protocol_wolgameres)

pvpgn_v3_add_test(test_protocol_wolgameres_wire_types
    SOURCES wire_types_test.cpp
    DEPS    protocol_wolgameres)
