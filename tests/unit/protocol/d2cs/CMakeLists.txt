pvpgn_v3_add_test(test_protocol_d2cs_codec
    SOURCES codec_test.cpp
    DEPS    protocol_d2cs)

pvpgn_v3_add_test(test_protocol_d2cs_ladderreply_encoder
    SOURCES ladderreply_encoder_test.cpp
    DEPS    protocol_d2cs)

pvpgn_v3_add_test(test_protocol_d2cs_charlistreply_encoder
    SOURCES charlistreply_encoder_test.cpp
    DEPS    protocol_d2cs)

pvpgn_v3_add_test(test_protocol_d2cs_bnetd_wire_types
    SOURCES bnetd_wire_types_test.cpp
    DEPS    protocol_d2cs)

pvpgn_v3_add_test(test_protocol_d2cs_wire_types
    SOURCES wire_types_test.cpp
    DEPS    protocol_d2cs)

# TODO: Fix d2cs_fsm test - D2CSSessionFsm not fully implemented
# pvpgn_v3_add_test(test_protocol_d2cs_fsm
#     SOURCES fsm_test.cpp
#     DEPS    protocol_d2cs)
