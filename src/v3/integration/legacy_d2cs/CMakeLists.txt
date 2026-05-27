# =============================================================================
# src/v3/integration/legacy_d2cs/CMakeLists.txt
# =============================================================================
# Added LATE in the top-level configure -- AFTER `add_subdirectory(src)` --
# so that the legacy `d2cs_legacy` target already exists when we reference
# it here. Mirrors the R193.fix repair done for legacy_bnetd. Done in R196.b.
#
#   Original (broken) order:
#     add_subdirectory(src/v3)   # v3 sees d2cs_legacy MISSING
#     add_subdirectory(src)      # creates d2cs_legacy too late
#
#   The v3 tree previously tried to create `integration_legacy_d2cs_linked`
#   under `if(TARGET d2cs_legacy)` at v3-time -- but d2cs_legacy did not exist
#   yet, so the if-block was FALSE and the target was never created. This in
#   turn made `if(TARGET integration_legacy_d2cs_linked)` FALSE in
#   src/d2cs/CMakeLists.txt, which meant PVPGN_V3_D2CS_INTEGRATION=1 was
#   never set on d2cs_legacy, which made every strangler `#ifdef` block dead
#   code under any WITH_D2CS=ON build. See:
#     plans/r196b-checklist.md
#
# Two responsibilities consolidated here (both used to live elsewhere and
# both were dead under the old order):
#
#   1. integration_legacy_d2cs_linked
#      Was at src/v3/CMakeLists.txt:1274 (moved here by R196.b).
#
#   2. d2cs / d2cs_legacy strangler wiring
#      target_link_libraries(d2cs PRIVATE integration_legacy_d2cs_linked)
#      target_compile_definitions(d2cs_legacy PRIVATE PVPGN_V3_D2CS_INTEGRATION=1)
#      Was at src/d2cs/CMakeLists.txt:58-65 (moved here by R196.b).
# =============================================================================

# ---------------------------------------------------------------------------
# 1. integration_legacy_d2cs_linked (the v3 static lib hookup target)
# ---------------------------------------------------------------------------
if(TARGET d2cs_legacy)
    pvpgn_v3_add_library(integration_legacy_d2cs_linked
        STATIC
        SOURCES
            src/send_packet_bridge_link.cpp
        PUBLIC_INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        PUBLIC_DEPS
            core
            integration_legacy_d2cs
        DEPS
            d2cs_legacy
    )
    # Legacy headers are not warning-clean under v3 strict warnings.
    target_compile_options(integration_legacy_d2cs_linked PRIVATE
        -w)
    # Legacy headers expect to be reached as `common/packet.h`,
    # `d2cs/connection.h`, etc -- i.e. relative to `src/`.
    target_include_directories(integration_legacy_d2cs_linked PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR})
endif()

# ---------------------------------------------------------------------------
# 2. d2cs / d2cs_legacy strangler wiring
# ---------------------------------------------------------------------------
# Previously gated on `if(TARGET integration_legacy_d2cs_linked)` in
# src/d2cs/CMakeLists.txt -- which was always FALSE under the broken
# ordering. Here we are in the post-src subdir, so both `d2cs` and
# `d2cs_legacy` are guaranteed to exist, and `integration_legacy_d2cs_linked`
# exists iff the if-block above fired.
if(TARGET d2cs AND TARGET integration_legacy_d2cs_linked)
    target_link_libraries(d2cs PRIVATE integration_legacy_d2cs_linked)
    target_compile_definitions(d2cs PRIVATE PVPGN_V3_D2CS_INTEGRATION=1)
    target_compile_definitions(d2cs_legacy PRIVATE PVPGN_V3_D2CS_INTEGRATION=1)
    target_include_directories(d2cs_legacy PRIVATE
        ${CMAKE_SOURCE_DIR}/src/v3/integration/legacy_d2cs/include)
endif()
