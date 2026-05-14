pvpgn_v3_add_test(test_protocol_bnet_codec
    SOURCES codec_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_fsm
    SOURCES fsm_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_anongame
    SOURCES anongame_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_protocol_bnet_anongame_tags
    SOURCES anongame_tags_test.cpp
    DEPS    protocol_bnet)
