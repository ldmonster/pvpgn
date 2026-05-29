pvpgn_v3_add_library(service_d2dbs
    SOURCES src/d2dbs_composition.cpp
    PUBLIC_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include
    DEPS pvpgn_v3_runtime pvpgn_v3_application_realm pvpgn_v3_protocol_d2dbs pvpgn_v3_core
)
