# Header-only session factory module
# Provides protocol-specific session factories (BNet, IRC, File, Telnet, WOL)
# that wire together TcpSession, context impls, and protocol FSMs.

pvpgn_v3_add_library(infra_session
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
        application_ports
        infra_net
        infra_routing
        protocol_bnet
        protocol_irc
        protocol_file
        protocol_telnet
        protocol_wolgameres
        domain_shared
)
