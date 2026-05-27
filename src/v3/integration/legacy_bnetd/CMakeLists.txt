# =============================================================================
# src/v3/integration/legacy_bnetd/CMakeLists.txt
# =============================================================================
# Added LATE in the top-level configure -- AFTER `add_subdirectory(src)` --
# so that the legacy `bnetd_legacy` target already exists when we reference
# it here. Repairs the R192/R193 systemic CMake ordering bug:
#
#   Original (broken) order:
#     add_subdirectory(src/v3)   # creates v3 tree
#     add_subdirectory(src)      # creates bnetd_legacy
#
#   The v3 tree previously tried to create `integration_legacy_bnetd_linked`
#   under `if(TARGET bnetd_legacy AND PVPGN_V3_WITH_BOOST)` at v3-time -- but
#   bnetd_legacy did not exist yet, so the if-block was FALSE and the target
#   was never created. This in turn made `if(TARGET integration_legacy_bnetd_linked)`
#   FALSE in src/bnetd/CMakeLists.txt, which meant PVPGN_V3_BNETD_INTEGRATION=1
#   was never set on bnetd_legacy, which made every strangler `#ifdef`
#   block dead code under any WITH_BNETD=ON build. See:
#     plans/r192-checklist.md
#     plans/r193-audit-checklist.md
#
# Two responsibilities consolidated here (both used to live elsewhere and
# both were dead under the old order):
#
#   1. integration_legacy_bnetd_linked
#      The v3 static lib that hooks UDP dispatch / install_v3_handlers
#      / all strangler bridges into bnetd_legacy. Was at
#      src/v3/CMakeLists.txt:1352 (moved by R193.fix).
#
#   2. bnetd / bnetd_legacy strangler wiring
#      target_link_libraries(bnetd PRIVATE integration_legacy_bnetd_linked)
#      target_compile_definitions(bnetd_legacy PRIVATE PVPGN_V3_BNETD_INTEGRATION=1)
#      and friends. Was at src/bnetd/CMakeLists.txt:121-156 (moved by R193.fix).
#
#   3. server_v3_hook injection onto bnetd_legacy
#      Adds src/bnetd/server_v3_hook.cpp + app/bnetd include path.
#      Was at src/v3/app/bnetd/CMakeLists.txt:100-117 (moved by R193.fix).
# =============================================================================

# ---------------------------------------------------------------------------
# 1. integration_legacy_bnetd_linked (the v3 static lib hookup target)
# ---------------------------------------------------------------------------
if(TARGET bnetd_legacy AND PVPGN_V3_WITH_BOOST)
    pvpgn_v3_add_library(integration_legacy_bnetd_linked
        STATIC
        SOURCES
            src/legacy_udp_dispatcher.cpp
            src/udp_bridge.cpp
            src/tcp_bridge.cpp
            src/dispatch.cpp
            src/anongame_inforeply_bridge.cpp
            src/icon_account_adapter.cpp
            src/get_icon_bridge.cpp
            src/set_icon_bridge.cpp
            src/tournament_bridge.cpp
            src/clan_profile_bridge.cpp
            src/profile_bridge.cpp
            src/chat_command_bridge.cpp
            src/legacy_chat_reply_sink_default_dispatch.cpp
            src/legacy_account_repository.cpp
            src/legacy_event_logger.cpp
            src/legacy_whisper_target_lookup.cpp
            src/install_v3_handlers.cpp
            src/legacy_bnet_frame_router_link.cpp
            src/send_packet_bridge_link.cpp
            src/init_conn_bridge_link.cpp
            src/init_packet_dispatch_link.cpp
            src/init_side_effects_link.cpp
            src/ads_bridge_link.cpp
            src/realm_list_bridge_link.cpp
        PUBLIC_INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/include
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
            application_bnet_packet_pump
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
    # `bnetd/handle_udp.h`, etc. -- i.e. relative to `src/`.
    target_include_directories(integration_legacy_bnetd_linked PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR})
endif()


# ---------------------------------------------------------------------------
# 2. bnetd / bnetd_legacy strangler wiring
# ---------------------------------------------------------------------------
# Previously gated on `if(TARGET integration_legacy_bnetd_linked)` in
# src/bnetd/CMakeLists.txt -- which was always FALSE under the broken
# ordering. Here we are in the post-src subdir, so both `bnetd` and
# `bnetd_legacy` are guaranteed to exist, and `integration_legacy_bnetd_linked`
# exists iff the if-block above fired.
if(TARGET bnetd AND TARGET integration_legacy_bnetd_linked)
    target_link_libraries(bnetd PRIVATE integration_legacy_bnetd_linked)
    # R194: the legacy `bnetd` exe has `server_v3_hook.cpp` injected
    # (see block 3 below) which references `LegacyBridge::instance()`
    # + `LegacyBridge::tick()`. Those live in `app_bnetd_legacy_bridge`
    # (R194 extract), so we link that small static lib too.
    if(TARGET app_bnetd_legacy_bridge)
        target_link_libraries(bnetd PRIVATE app_bnetd_legacy_bridge)
    endif()
    if(TARGET infra_log_simple)
        # JsonLineLogger (Batch 23c) is referenced from server.cpp
        # behind PVPGN_V3_BNETD_INTEGRATION; link the lib + expose its
        # public include path on the legacy compile unit.
        target_link_libraries(bnetd PRIVATE infra_log_simple)
        target_include_directories(bnetd_legacy PRIVATE
            ${CMAKE_SOURCE_DIR}/src/v3/infra/log/include)
    endif()
    # Batch 26c: PasswordRotationObserver + InMemoryEventBus are
    # referenced from server.cpp behind PVPGN_V3_AUDIT_LOG.
    if(TARGET application_auth)
        target_link_libraries(bnetd PRIVATE application_auth)
    endif()
    target_compile_definitions(bnetd PRIVATE PVPGN_V3_BNETD_INTEGRATION=1)
    # bnetd_legacy itself hosts the legacy `_client_anongame_*` /
    # `handle_command` etc. handlers that contain the
    # `#ifdef PVPGN_V3_BNETD_INTEGRATION` strangler hooks. Without the
    # define on the lib those hooks were dead code (compiled-out) even
    # when the bridges were linked into the EXE. Enable them here and
    # expose the v3 strangler macro header on the legacy include path.
    target_compile_definitions(bnetd_legacy PRIVATE PVPGN_V3_BNETD_INTEGRATION=1)
    target_include_directories(bnetd_legacy PRIVATE
        ${CMAKE_SOURCE_DIR}/src/v3/integration/legacy_bnetd/include
        ${CMAKE_SOURCE_DIR}/src/v3/application/chat/include
        ${CMAKE_SOURCE_DIR}/src/v3/application/i18n/include
        ${CMAKE_SOURCE_DIR}/src/v3/application/auth/include
        ${CMAKE_SOURCE_DIR}/src/v3/application/ports/include
        ${CMAKE_SOURCE_DIR}/src/v3/domain/shared/include
        ${CMAKE_SOURCE_DIR}/src/v3/domain/identity/include
        ${CMAKE_SOURCE_DIR}/src/v3/infra/inmemory/include
        ${CMAKE_SOURCE_DIR}/src/v3/core/include)
endif()


# ---------------------------------------------------------------------------
# 3. server_v3_hook.cpp injection onto bnetd_legacy
# ---------------------------------------------------------------------------
# Adds src/bnetd/server_v3_hook.cpp to the bnetd_legacy static library
# when the v3 build is active. The hook is compiled with
# PVPGN_V3_BNETD_INTEGRATION=1 (set above when integration_legacy_bnetd_linked
# is built), so the non-stub path is compiled in.
if(TARGET bnetd_legacy)
    target_sources(bnetd_legacy PRIVATE
        ${CMAKE_SOURCE_DIR}/src/bnetd/server_v3_hook.cpp
    )
    target_include_directories(bnetd_legacy PRIVATE
        ${CMAKE_SOURCE_DIR}/src/v3/app/bnetd/include
    )
endif()
