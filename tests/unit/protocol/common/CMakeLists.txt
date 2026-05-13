pvpgn_v3_add_test(test_protocol_packet
    SOURCES packet_test.cpp
    DEPS    protocol_common)
pvpgn_v3_add_test(test_protocol_reader
    SOURCES reader_test.cpp
    DEPS    protocol_common)
pvpgn_v3_add_test(test_protocol_writer
    SOURCES writer_test.cpp
    DEPS    protocol_common)
pvpgn_v3_add_test(test_protocol_replay
    SOURCES replay_test.cpp
    DEPS    protocol_common protocol_bnet)
