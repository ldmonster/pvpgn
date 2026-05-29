# Header-only routing infrastructure module
# Provides IMessageRouter and MessageRouterImpl for session management.

pvpgn_v3_add_library(infra_routing
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
        application_ports
)
