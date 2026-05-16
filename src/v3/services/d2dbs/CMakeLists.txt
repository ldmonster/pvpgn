pvpgn_v3_add_library(service_d2dbs
    SOURCES src/d2dbs_composition.cpp
    HEADERS include/services/d2dbs/d2dbs_composition.hpp
    DEPS pvpgn_v3_runtime pvpgn_v3_application_realm pvpgn_v3_protocol_d2dbs pvpgn_v3_core
)
