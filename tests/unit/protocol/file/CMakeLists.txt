pvpgn_v3_add_test(test_protocol_file_codec
    SOURCES codec_test.cpp
    DEPS    protocol_file)

pvpgn_v3_add_test(test_protocol_file_wire_types
    SOURCES wire_types_test.cpp
    DEPS    protocol_file)

pvpgn_v3_add_test(test_protocol_file_bnftp_fsm
    SOURCES bnftp_fsm_test.cpp
    DEPS    protocol_file)
