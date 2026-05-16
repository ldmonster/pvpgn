# Root for the v3 sub-tree.
# Built only when PVPGN_BUILD_V3=ON (legacy default keeps building untouched).

include(${CMAKE_SOURCE_DIR}/cmake/v3.cmake)
include(FetchContent)

# ---- third-party (FetchContent, header/source only) ---------------------

option(PVPGN_V3_WITH_SPDLOG  "Fetch & build spdlog for the v3 tree"      ON)
option(PVPGN_V3_WITH_TOMLPP  "Fetch & build toml++ for the v3 tree"      ON)
option(PVPGN_V3_WITH_BOOST   "Use system Boost (Asio + optional Fiber)"  ON)
option(PVPGN_V3_WITH_FIBER   "Enable Boost.Fiber-backed awaiters"        OFF)
option(PVPGN_V3_WITH_ANKERL  "Fetch & build ankerl/unordered_dense"      ON)

if(PVPGN_V3_WITH_ANKERL)
    FetchContent_Declare(
        ankerl_unordered_dense
        GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
        GIT_TAG        v4.4.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(ankerl_unordered_dense)
endif()

if(PVPGN_V3_WITH_SPDLOG)
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(SPDLOG_INSTALL      OFF CACHE BOOL "" FORCE)
    set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.14.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(spdlog)
    foreach(_t spdlog spdlog_header_only)
        if(TARGET ${_t})
            set_target_properties(${_t} PROPERTIES SYSTEM TRUE)
        endif()
    endforeach()
endif()

if(PVPGN_V3_WITH_TOMLPP)
    FetchContent_Declare(
        tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG        v3.4.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(tomlplusplus)
    if(TARGET tomlplusplus_tomlplusplus)
        set_target_properties(tomlplusplus_tomlplusplus PROPERTIES SYSTEM TRUE)
    endif()
endif()

if(PVPGN_V3_WITH_BOOST)
    set(_pvpgn_boost_components system)
    if(PVPGN_V3_WITH_FIBER)
        list(APPEND _pvpgn_boost_components fiber context)
    endif()
    find_package(Boost 1.75 REQUIRED COMPONENTS ${_pvpgn_boost_components})
endif()

# Find OpenSSL for runtime peer_link support
find_package(OpenSSL REQUIRED)

# ---- core ---------------------------------------------------------------

pvpgn_v3_add_library(core
    STATIC
    SOURCES
        core/src/logging.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/core/include
)

# ---- runtime (service host, peer-to-peer communication) -------------------

pvpgn_v3_add_library(runtime
    STATIC
    SOURCES
        runtime/src/service_host.cpp
        runtime/src/peer_link.cpp
        runtime/src/cli.cpp
        runtime/src/daemonize.cpp
        runtime/src/crash_handler.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime/include
    PUBLIC_DEPS
        core
)

# Platform-specific runtime sources
if(WIN32)
    target_sources(runtime PRIVATE runtime/src/win_service.cpp)
endif()

# ---- infra/log ----------------------------------------------------------

# Always-available structured logger backends (no third-party deps).
pvpgn_v3_add_library(infra_log_simple
    STATIC
    SOURCES
        infra/log/src/json_line_logger.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/log/include
    PUBLIC_DEPS
        core
)

if(PVPGN_V3_WITH_SPDLOG)
    pvpgn_v3_add_library(infra_log
        STATIC
        SOURCES
            infra/log/src/spdlog_logger.cpp
        PUBLIC_INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/infra/log/include
        PUBLIC_DEPS
            core
        DEPS
            spdlog::spdlog
    )
endif()

# ---- infra/config -------------------------------------------------------

if(PVPGN_V3_WITH_TOMLPP)
    pvpgn_v3_add_library(infra_config
        STATIC
        SOURCES
            infra/config/src/server_config.cpp
            infra/config/src/config_watcher.cpp
        PUBLIC_INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/infra/config/include
        PUBLIC_DEPS
            core
            application_ports
        DEPS
            tomlplusplus::tomlplusplus
    )
endif()

# ---- infra/compression --------------------------------------------------
#
# Thin zlib adapter for the legacy FINDANONGAME INFOREPLY framing.
# Kept out of `protocol_bnet` so the protocol layer stays free of any
# external compression dependency.

option(PVPGN_V3_FETCH_ZLIB
       "Fetch & build zlib for the v3 tree if not already available"
       ON)

if(NOT TARGET ZLIB::ZLIB)
    find_package(ZLIB QUIET)
endif()

if(NOT TARGET ZLIB::ZLIB AND PVPGN_V3_FETCH_ZLIB)
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SKIP_INSTALL_ALL    ON  CACHE BOOL "" FORCE)
    FetchContent_Declare(
        zlib_vendored
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG        v1.3.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(zlib_vendored)
    if(TARGET zlibstatic AND NOT TARGET ZLIB::ZLIB)
        # zlibstatic does not auto-propagate its source-dir include path
        # to consumers; do it explicitly.
        target_include_directories(zlibstatic
            PUBLIC
                $<BUILD_INTERFACE:${zlib_vendored_SOURCE_DIR}>
                $<BUILD_INTERFACE:${zlib_vendored_BINARY_DIR}>)
        set_target_properties(zlibstatic PROPERTIES SYSTEM TRUE)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
    endif()
endif()

if(NOT TARGET ZLIB::ZLIB)
    find_package(ZLIB REQUIRED)
endif()

pvpgn_v3_add_library(infra_compression
    STATIC
    SOURCES
        infra/compression/src/zlib_anongame.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/compression/include
    PUBLIC_DEPS
        core
    DEPS
        ZLIB::ZLIB
)

# ---- infra/scripting/plugin -------------------------------------------------

add_subdirectory(infra/scripting)

# ---- protocol/common ----------------------------------------------------

pvpgn_v3_add_library(protocol_common
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/common/include
    PUBLIC_DEPS
        core
)

# ---- protocol/bnet ------------------------------------------------------

pvpgn_v3_add_library(protocol_bnet
    STATIC
    SOURCES
        protocol/bnet/src/codec.cpp
        protocol/bnet/src/fsm.cpp
        protocol/bnet/src/anongame.cpp
        protocol/bnet/src/anongame_tags.cpp
        protocol/bnet/src/event_dispatcher.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/bnet/include
    PUBLIC_DEPS
        core
        protocol_common
        application_ports
        application_auth
        application_chat
        application_game
        application_moderation
)

# MSVC: codec.cpp now exceeds the default object-file section count.
if(MSVC)
    target_compile_options(protocol_bnet PRIVATE /bigobj)
endif()

# ---- protocol/irc -------------------------------------------------------

pvpgn_v3_add_library(protocol_irc
    STATIC
    SOURCES
        protocol/irc/src/codec.cpp
        protocol/irc/src/fsm.cpp
        protocol/irc/src/bridge_fsm.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/irc/include
    PUBLIC_DEPS
        core
        protocol_common
        application_chat
        application_auth
)

# ---- protocol/udp -------------------------------------------------------

pvpgn_v3_add_library(protocol_udp
    STATIC
    SOURCES
        protocol/udp/src/codec.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/udp/include
    PUBLIC_DEPS
        core
        protocol_common
)

# ---- protocol/telnet ----------------------------------------------------

pvpgn_v3_add_library(protocol_telnet
    STATIC
    SOURCES
        protocol/telnet/src/codec.cpp
        # protocol/telnet/src/admin_fsm.cpp  # DISABLED: missing command_registry.hpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/telnet/include
    PUBLIC_DEPS
        core
        protocol_common
        application_ports
)

# ---- protocol/file ------------------------------------------------------

pvpgn_v3_add_library(protocol_file
    STATIC
    SOURCES
        protocol/file/src/codec.cpp
        # protocol/file/src/bnftp_fsm.cpp  # DISABLED: missing session_context.hpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/file/include
    PUBLIC_DEPS
        core
        protocol_common
)

# ---- protocol/d2cs ------------------------------------------------------

pvpgn_v3_add_library(protocol_d2cs
    STATIC
    SOURCES
        protocol/d2cs/src/codec.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/d2cs/include
    PUBLIC_DEPS
        core
        protocol_common
)

# ---- protocol/d2gs ------------------------------------------------------

pvpgn_v3_add_library(protocol_d2gs
    STATIC
    SOURCES
        protocol/d2gs/src/codec.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/d2gs/include
    PUBLIC_DEPS
        core
        protocol_common
)

# ---- protocol/d2dbs -----------------------------------------------------

pvpgn_v3_add_library(protocol_d2dbs
    STATIC
    SOURCES
        protocol/d2dbs/src/fsm.cpp
        protocol/d2dbs/src/codec.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/d2dbs/include
    PUBLIC_DEPS
        core
        protocol_common
)

# ---- protocol/wolgameres ------------------------------------------------

pvpgn_v3_add_library(protocol_wolgameres
    STATIC
    SOURCES
        protocol/wolgameres/src/codec.cpp
        protocol/wolgameres/src/wol_fsm.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/wolgameres/include
    PUBLIC_DEPS
        core
        protocol_common
        application_ports
)

# ---- domain/shared (value objects, events, IDs) -------------------------

pvpgn_v3_add_library(domain_shared
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/shared/include
    PUBLIC_DEPS
        core
)

# ---- domain/identity (Account aggregate) --------------------------------

pvpgn_v3_add_library(domain_identity
    SOURCES
        domain/identity/src/attribute_map.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/identity/include
    PUBLIC_DEPS
        core
        domain_shared
)

# ---- domain/chat (Channel aggregate) ------------------------------------

pvpgn_v3_add_library(domain_chat
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/chat/include
    PUBLIC_DEPS
        core
        domain_shared
)

# ---- domain/social (FriendList, Clan) -----------------------------------

pvpgn_v3_add_library(domain_social
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/social/include
    PUBLIC_DEPS
        core
        domain_shared
)

# ---- domain/gameplay (Game aggregate + FSM) -----------------------------

pvpgn_v3_add_library(domain_gameplay
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/gameplay/include
    PUBLIC_DEPS
        core
        domain_shared
)

# ---- domain/ladder (LadderCalculator service) ---------------------------

pvpgn_v3_add_library(domain_ladder
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/ladder/include
    PUBLIC_DEPS
        core
        domain_shared
)

# ---- domain/moderation (IpBanList) --------------------------------------

pvpgn_v3_add_library(domain_moderation
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/moderation/include
    PUBLIC_DEPS
        core
        domain_shared
)

# ---- domain/matchmaking (AnonGameQueue, Tournament) ---------------------

pvpgn_v3_add_library(domain_matchmaking
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/matchmaking/include
    PUBLIC_DEPS
        core
        domain_shared
)

# ---- domain/realm (Realm + Character) -----------------------------------

pvpgn_v3_add_library(domain_realm
    STATIC
    SOURCES
        domain/realm/src/character.cpp
        domain/realm/src/dupe_checker.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/realm/include
    PUBLIC_DEPS
        core
        domain_shared
)

# ---- application/ports (interface-only) ---------------------------------

pvpgn_v3_add_library(application_ports
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/ports/include
    PUBLIC_DEPS
        core
        domain_shared
        domain_identity
        domain_chat
        domain_gameplay
        domain_moderation
        domain_social
)

# ---- application/auth (LoginUser) ---------------------------------------

pvpgn_v3_add_library(application_auth
    STATIC
    SOURCES
        application/auth/src/login_user.cpp
        application/auth/src/login_classifier.cpp
        application/auth/src/password_rotation_observer.cpp
        application/auth/src/change_password.cpp
        application/auth/src/create_account.cpp
        application/auth/src/logout_user.cpp
        application/auth/src/account_lock.cpp
        application/auth/src/permission_checker.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/auth/include
    PUBLIC_DEPS
        core
        application_ports
        domain_shared
        domain_identity
        domain_moderation
)


# ---- application/realm (CharacterLock, GameServerQueue) --------------------

pvpgn_v3_add_library(application_realm
    STATIC
    SOURCES
        application/realm/src/character_lock.cpp
        application/realm/src/gs_queue.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/realm/include
    PUBLIC_DEPS
        core
        domain_realm
)
# ---- application/anongame_infoply ---------------------------------------
#
# Composes a full FINDANONGAME INFOREPLY set: typed payloads ->
# serialize -> zlib-compress -> AnonGameInfoReply envelopes.

pvpgn_v3_add_library(application_anongame_infoply
    STATIC
    SOURCES
        application/anongame_infoply/src/inforeply_builder.cpp
        application/anongame_infoply/src/type_composer.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/anongame_infoply/include
    PUBLIC_DEPS
        core
        protocol_bnet
        infra_compression
)

# ---- application/icon_table ---------------------------------------------
#
# Pure builder for the FINDANONGAME GET_ICON (sub-option 0x09) reply
# table. Mirrors `_client_anongame_get_icon` logic but accepts all
# account state as inputs so it can be tested without the legacy
# globals. Depends on infra/legacy_config for the `IconReqTable`
# threshold table type.

pvpgn_v3_add_library(application_icon_table
    STATIC
    SOURCES
        application/icon_table/src/icon_table.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/icon_table/include
    PUBLIC_DEPS
        core
)

# ---- application/tournament ---------------------------------------------
#
# Pure builder for the FINDANONGAME 0x07 (TOURNAMENT) reply. Mirrors
# the legacy `_client_anongame_tournament` state machine but takes
# every input as a value so the logic can be exercised in isolation.

pvpgn_v3_add_library(application_tournament
    STATIC
    SOURCES
        application/tournament/src/tournament_reply.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/tournament/include
    PUBLIC_DEPS
        core
        protocol_bnet
)

# ---- application/profile ------------------------------------------------
#
# Pure builder for the FINDANONGAME 0x04 (PROFILE) reply. Mirrors the
# legacy `_client_anongame_profile` ladder/race/AT-team assembly but
# accepts a snapshot value as input so it can be tested without bnetd.

pvpgn_v3_add_library(application_profile
    STATIC
    SOURCES
        application/profile/src/profile_reply.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/profile/include
    PUBLIC_DEPS
        core
        protocol_bnet
)

# ---- application/chat ---------------------------------------------------
#
# Pure classifier for CLIENT_CHATCOMMAND payloads. Splits the raw chat
# input into Whisper / Command / ChannelMessage / Empty so the future
# CHAT bridge can route deterministically without depending on the
# legacy `t_connection`. Scaffold for the CHAT vertical (Batch 17a).

pvpgn_v3_add_library(application_chat
    STATIC
    SOURCES
        application/chat/src/chat_command.cpp
        application/chat/src/whisper_use_case.cpp
        application/chat/src/join_channel.cpp
        application/chat/src/post_message.cpp
        application/chat/src/leave_channel.cpp
        application/chat/src/list_channels.cpp
        application/chat/src/send_emote.cpp
        application/chat/src/kick_from_channel.cpp
        application/chat/src/ban_from_channel.cpp
        application/chat/src/set_channel_topic.cpp
        application/chat/src/command_registry.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/chat/include
    PUBLIC_DEPS
        core
        application_ports
        domain_chat
)

# ---- application/game ---------------------------------------------------
#
# Game management use-cases (StartGame, JoinGame, LeaveGame).
# Orchestrates between repositories and the Game aggregate.

pvpgn_v3_add_library(application_game
    STATIC
    SOURCES
        application/game/src/start_game.cpp
        application/game/src/join_game.cpp
        application/game/src/leave_game.cpp
        application/game/src/report_game_result.cpp
        application/game/src/list_public_games.cpp
        application/game/src/create_private_game.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/game/include
    PUBLIC_DEPS
        core
        application_ports
        domain_gameplay
)

# ---- application/moderation -----------------------------------------------
#
# Moderation use-cases (CheckIpBan).
# Checks IP ban status via the IpBanRepository port.

pvpgn_v3_add_library(application_moderation
    STATIC
    SOURCES
        application/moderation/src/check_ip_ban.cpp
        application/moderation/src/ban_account.cpp
        application/moderation/src/unban_account.cpp
        application/moderation/src/ban_ip.cpp
        application/moderation/src/kick_connection.cpp
        application/moderation/src/silence_user.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/moderation/include
    PUBLIC_DEPS
        core
        application_ports
        domain_moderation
)

# ---- application/social --------------------------------------------------
#
# Social use-cases: friend list management, clan operations.

pvpgn_v3_add_library(application_social
    STATIC
    SOURCES
        application/social/src/add_friend.cpp
        application/social/src/remove_friend.cpp
        application/social/src/list_friends.cpp
        application/social/src/create_clan.cpp
        application/social/src/disband_clan.cpp
        application/social/src/invite_to_clan.cpp
        application/social/src/kick_from_clan.cpp
        application/social/src/promote_clan_member.cpp
        application/social/src/set_clan_motd.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/social/include
    PUBLIC_DEPS
        core
        application_ports
        domain_social
)

# ---- application/i18n ---------------------------------------------------

pvpgn_v3_add_library(application_i18n
    STATIC
    SOURCES
        application/i18n/src/string_table.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/i18n/include
    PUBLIC_DEPS
        core
)

# ---- infra/legacy_config (parse legacy bnetd .conf files) ---------------
#
# Bridge adapters that convert legacy bnetd config-file dialects into the
# typed v3 snapshots used by the application services.

pvpgn_v3_add_library(infra_legacy_config
    STATIC
    SOURCES
        infra/legacy_config/src/anongame_infos_loader.cpp
        infra/legacy_config/src/anongame_maplists_loader.cpp
        infra/legacy_config/src/icon_req_loader.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/legacy_config/include
    PUBLIC_DEPS
        core
        protocol_bnet
        application_anongame_infoply
        application_icon_table
)

# ---- infra/audit (audit log implementations) --------------------------------

pvpgn_v3_add_library(infra_audit
    STATIC
    SOURCES
        infra/audit/src/in_memory_audit_log.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/audit/include
    PUBLIC_DEPS
        core
        application_ports
)

# ---- infra/inmemory (test + dev adapters, header-only) ------------------

pvpgn_v3_add_library(infra_inmemory
    STATIC
    SOURCES
        infra/inmemory/src/unit_of_work_factory.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/inmemory/include
        ${ankerl_unordered_dense_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
        application_ports
        domain_shared
        domain_identity
        domain_realm
        domain_ladder
)

# ---- infra/storage/repository (Phase 2 in-memory repositories) -----------

add_subdirectory(infra/storage/repository)

# ---- infra/routing (message routing layer) -------------------------------

pvpgn_v3_add_library(infra_routing
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/routing/include
    PUBLIC_DEPS
        core
        application_ports
)

# ---- infra/session (protocol-specific session factories) -----------------

pvpgn_v3_add_library(infra_session
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/session/include
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

# ---- infra/legacy_crypto (bnet_hash adapter) ----------------------------
#
# Adapts the legacy `pvpgn::bnet_hash` (broken-SHA-1) implementation
# to the v3 `IPasswordHasher` port. Linked against the legacy
# `common` library so we never reimplement the algorithm.

if(TARGET common)
    pvpgn_v3_add_library(infra_legacy_crypto
        STATIC
        SOURCES
            infra/legacy_crypto/src/bnet_session_hasher.cpp
        PUBLIC_INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/infra/legacy_crypto/include
        PUBLIC_DEPS
            core
            application_ports
            domain_shared
        DEPS
            common
    )
    # Legacy `common/setup_before.h` is not warning-clean under /WX.
    target_compile_options(infra_legacy_crypto PRIVATE -w)
    target_include_directories(infra_legacy_crypto PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR})
endif()

# ---- integration/legacy_bnetd (strangler-fig adapter) -------------------

pvpgn_v3_add_library(integration_legacy_bnetd
    STATIC
    SOURCES
        integration/legacy_bnetd/src/legacy_protocol_handler.cpp
        integration/legacy_bnetd/src/legacy_bnet_frame_router.cpp
        integration/legacy_bnetd/src/bnet_strangler_handler.cpp
        integration/legacy_bnetd/src/anongame_bootstrap.cpp
        integration/legacy_bnetd/src/bridge_logger.cpp
        integration/legacy_bnetd/src/legacy_chat_reply_sink.cpp
        integration/legacy_bnetd/src/login_user_bridge.cpp
        integration/legacy_bnetd/src/change_password_bridge.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/integration/legacy_bnetd/include
    PUBLIC_DEPS
        core
        application_ports
        application_anongame_infoply
        application_chat
        application_i18n
        infra_legacy_config
        protocol_common
        protocol_bnet
)

# ---- integration/legacy_bnetd/linked (real bnetd_legacy hookup) ---------
#
# Only built when the legacy bnetd library is in the same configure
# AND boost is available, because the UDP dispatch path needs asio.
# Provides `LegacyUdpDispatcher` which actually invokes the legacy
# `pvpgn::bnetd::handle_udp_packet`, plus all the strangler-fig
# bridges and `install_v3_handlers` registrations. The consumer is
# responsible for initialising the legacy globals before any
# datagram is delivered.

if(TARGET bnetd_legacy AND PVPGN_V3_WITH_BOOST)
    pvpgn_v3_add_library(integration_legacy_bnetd_linked
        STATIC
        SOURCES
            integration/legacy_bnetd/src/legacy_udp_dispatcher.cpp
            integration/legacy_bnetd/src/udp_bridge.cpp
            integration/legacy_bnetd/src/tcp_bridge.cpp
            integration/legacy_bnetd/src/dispatch.cpp
            integration/legacy_bnetd/src/anongame_inforeply_bridge.cpp
            integration/legacy_bnetd/src/icon_account_adapter.cpp
            integration/legacy_bnetd/src/get_icon_bridge.cpp
            integration/legacy_bnetd/src/set_icon_bridge.cpp
            integration/legacy_bnetd/src/tournament_bridge.cpp
            integration/legacy_bnetd/src/clan_profile_bridge.cpp
            integration/legacy_bnetd/src/profile_bridge.cpp
            integration/legacy_bnetd/src/chat_command_bridge.cpp
            integration/legacy_bnetd/src/legacy_chat_reply_sink_default_dispatch.cpp
            integration/legacy_bnetd/src/legacy_account_repository.cpp
            integration/legacy_bnetd/src/legacy_event_logger.cpp
            integration/legacy_bnetd/src/legacy_whisper_target_lookup.cpp
            integration/legacy_bnetd/src/install_v3_handlers.cpp
            integration/legacy_bnetd/src/legacy_bnet_frame_router_link.cpp
        PUBLIC_INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/integration/legacy_bnetd/include
        PUBLIC_DEPS
            core
            integration_legacy_bnetd
            infra_net
            application_icon_table
            application_tournament
            application_profile
            application_chat
            application_i18n
            application_auth
            infra_inmemory
            infra_legacy_crypto
        DEPS
            bnetd_legacy
    )
    # The legacy headers are not warning-clean under the v3 strict
    # warning set; suppress for this TU only.
    target_compile_options(integration_legacy_bnetd_linked PRIVATE
        -w)
    # bnetd_legacy headers expect to be reached as `common/packet.h`,
    # `bnetd/handle_udp.h`, etc. — i.e. relative to `src/`.
    target_include_directories(integration_legacy_bnetd_linked PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR})
endif()

# ---- infra/net ----------------------------------------------------------

if(PVPGN_V3_WITH_BOOST)
    set(_pvpgn_net_public_deps core Boost::system Threads::Threads)
    set(_pvpgn_net_deps)
    if(PVPGN_V3_WITH_FIBER)
        list(APPEND _pvpgn_net_public_deps Boost::fiber Boost::context)
    endif()
    find_package(Threads REQUIRED)
    pvpgn_v3_add_library(infra_net
        STATIC
        SOURCES
            infra/net/src/io_runtime.cpp
            infra/net/src/tcp_acceptor.cpp
            infra/net/src/tcp_session.cpp
            infra/net/src/udp_endpoint.cpp
            infra/net/src/fiber_pool.cpp
            infra/net/src/signal_handler.cpp
            infra/net/src/shutdown_coordinator.cpp
        PUBLIC_INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/infra/net/include
        PUBLIC_DEPS
            ${_pvpgn_net_public_deps}
            application_ports
            infra_config
        DEPS
            ${_pvpgn_net_deps}
    )
    if(PVPGN_V3_WITH_FIBER)
        target_compile_definitions(infra_net PUBLIC PVPGN_V3_HAVE_FIBER=1)
    endif()
    # Asio's any_executor equality operator trips -Wnull-dereference under
    # GCC; the access is in fact guarded but the analyser cannot see it.
    # Relaxed PUBLICly so consumers of infra_net inherit the suppression.
    # MSVC has no equivalent flag and rejects the GCC spelling outright,
    # so guard on compiler.
    if(NOT MSVC)
        target_compile_options(infra_net PUBLIC -Wno-null-dereference)
    endif()
endif()

# ---- infra/metrics (observability: Prometheus metrics, HTTP /metrics endpoint) --

pvpgn_v3_add_library(infra_metrics
    STATIC
    SOURCES
        infra/metrics/src/in_memory_metrics_registry.cpp
        infra/metrics/src/server_metrics.cpp
        # infra/metrics/src/http_metrics_server.cpp  # Disabled: uses SPDLOG macros without proper include
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/metrics/include
    PUBLIC_DEPS
        core
        application_ports
        infra_net
    DEPS
        Boost::system
)

# ---- infra/webui (REST API server + HTML dashboard) -----------------------

if(PVPGN_V3_WITH_BOOST)
    pvpgn_v3_add_library(infra_webui
        STATIC
        SOURCES
            infra/webui/src/web_server.cpp
        PUBLIC_INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/infra/webui/include
        PUBLIC_DEPS
            core
            application_ports
            infra_net
        DEPS
            Boost::system
    )
endif()

# ---- tools (utilities for migration and administration) ----------------------

add_subdirectory(tools/conf_converter)

# ---- tests --------------------------------------------------------------

if(PVPGN_V3_BUILD_TESTS)
    # Set up Catch2 for all test subdirectories
    include(FetchContent)
    
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    
    # Ensure Catch2 is built with C++20 to support std::string_view
    foreach(_t Catch2 Catch2WithMain)
        if(TARGET ${_t})
            target_compile_features(${_t} PUBLIC cxx_std_20)
            set_target_properties(${_t} PROPERTIES SYSTEM TRUE)
        endif()
    endforeach()
    
    enable_testing()
    include(CTest)
    
    add_subdirectory(
        ${CMAKE_SOURCE_DIR}/tests/unit
        ${CMAKE_BINARY_DIR}/tests/unit)
    add_subdirectory(
        ${CMAKE_SOURCE_DIR}/tests/functional
        ${CMAKE_BINARY_DIR}/tests/functional)
    add_subdirectory(
        ${CMAKE_SOURCE_DIR}/tests/e2e
        ${CMAKE_BINARY_DIR}/tests/e2e)
endif()
