pvpgn_v3_add_library(service_d2dbs
    SOURCES src/d2dbs_composition.cpp
    PUBLIC_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include
    DEPS runtime application_realm protocol_d2dbs infra_persistence_realm core
)
