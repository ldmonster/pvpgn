pvpgn_v3_add_library(protocol_d2dbs
    SOURCES src/fsm.cpp src/codec.cpp
    HEADERS include/protocol/d2dbs/fsm.hpp include/protocol/d2dbs/codec.hpp
    DEPS core
)
