pvpgn_v3_add_library(protocol_d2save
    SOURCES src/codec.cpp
    PUBLIC_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS core
)
