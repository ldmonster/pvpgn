pvpgn_v3_add_test(test_protocol_wol_fsm
    SOURCES wol_fsm_test.cpp
    DEPS    protocol_wol)

pvpgn_v3_add_test(test_protocol_wol_codec
    SOURCES codec_test.cpp
    DEPS    protocol_wol)

pvpgn_v3_add_test(test_protocol_wol_fsm_auth
    SOURCES wol_fsm_auth_test.cpp
    DEPS    protocol_wol application_auth infra_inmemory)

pvpgn_v3_add_test(test_protocol_wol_fsm_channel
    SOURCES wol_fsm_channel_test.cpp
    DEPS    protocol_wol)
