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

# R213a: every public header in src/v3/protocol/common/include/protocol/common/
# must compile in isolation.
pvpgn_v3_add_header_selfcheck(test_protocol_common_headers_selfcontained
    INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src/protocol/common/include
    HEADERS
        protocol/common/packet.hpp
        protocol/common/reader.hpp
        protocol/common/replay.hpp
        protocol/common/writer.hpp
    DEPS protocol_common
)
