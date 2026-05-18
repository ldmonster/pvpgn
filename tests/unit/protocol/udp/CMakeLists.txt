pvpgn_v3_add_test(test_protocol_udp_codec
    SOURCES codec_test.cpp
    DEPS    protocol_udp)

pvpgn_v3_add_test(test_protocol_udp_wire_types
    SOURCES wire_types_test.cpp
    DEPS    protocol_udp)
