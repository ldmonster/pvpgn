# src/v3/services/bnetd/CMakeLists.txt
#
# Defines the `services_bnetd` static library — the composition root for
# the bnetd server.
#
# BnetdService accepts IUnitOfWorkFactory&, IEventLoop&, INlsCredentialStore&,
# IChannelRepository&, IAccountRepository&, and ISessionRegistry& via
# constructor injection and provides run()/stop() lifecycle methods.
# It owns LoginUserNls, JoinChannel, PostMessage, LeaveChannel, and
# ListChannels use-case instances.
# It is linked by the `pvpgn_v3_bnetd` executable target in
# app/bnetd/CMakeLists.txt.
#
# Dependencies
# ------------
#   application_ports       — IUnitOfWorkFactory, IEventLoop, IChannelRepository,
#                             IAccountRepository, ISessionRegistry port interfaces
#   application_auth        — LoginUserNls, INlsCredentialStore (R285)
#   application_chat        — JoinChannel, PostMessage, LeaveChannel, ListChannels (R304)
#   domain_chat             — Channel aggregate, ChannelPolicy, ChannelFlag
#
# Optional (future Phase3 wiring):
#   infra_inmemory     — InMemoryUnitOfWorkFactory
#   infra_net          — IoRuntime, TcpAcceptor (Boost.Asio)

pvpgn_v3_add_library(services_bnetd
    SOURCES
        src/bnetd_service.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        application_ports
        application_auth
        application_chat
        domain_chat
        protocol_bnet
)
