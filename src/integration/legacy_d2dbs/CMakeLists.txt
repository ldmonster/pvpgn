# =============================================================================
# src/v3/integration/legacy_d2dbs/CMakeLists.txt
# =============================================================================
# Added LATE in the top-level configure -- AFTER `add_subdirectory(src)` --
# so that the legacy `d2dbs_legacy` target already exists when we reference
# it here. Mirrors the R193.fix / R196.b repair done for legacy_bnetd /
# legacy_d2cs. Done in R199.
#
#   Original (broken) order:
#     add_subdirectory(src/v3)   # v3 sees d2dbs_legacy MISSING
#     add_subdirectory(src)      # creates d2dbs_legacy too late
#
#   The v3 tree previously tried to create `integration_legacy_d2dbs_linked`
#   under `if(TARGET d2dbs_legacy)` at v3-time -- but d2dbs_legacy did not
#   exist yet, so the if-block was FALSE and the target was never created.
#   Same R193-class bug as R196.b's d2cs lane. See:
#     plans/r199-checklist.md
#
# Two responsibilities consolidated here (both used to live elsewhere and
# both were dead under the old order):
#
#   1. integration_legacy_d2dbs_linked
#      Was at src/v3/CMakeLists.txt:1296 (moved here by R199).
#
#   2. d2dbs / d2dbs_legacy strangler wiring
#      target_link_libraries(d2dbs PRIVATE integration_legacy_d2dbs_linked)
#      target_compile_definitions(d2dbs_legacy PRIVATE PVPGN_V3_D2DBS_INTEGRATION=1)
#      Was at src/d2dbs/CMakeLists.txt (moved here by R199).
# =============================================================================

# ---------------------------------------------------------------------------
# 1. integration_legacy_d2dbs_linked (the v3 static lib hookup target)
# ---------------------------------------------------------------------------
if(TARGET d2dbs_legacy)
    pvpgn_v3_add_library(integration_legacy_d2dbs_linked
        STATIC
        SOURCES
            src/send_packet_bridge_link.cpp
        PUBLIC_INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        PUBLIC_DEPS
            core
            integration_legacy_d2dbs
        DEPS
            d2dbs_legacy
    )
    # Legacy headers are not warning-clean under v3 strict warnings.
    target_compile_options(integration_legacy_d2dbs_linked PRIVATE
        -w)
    # Legacy headers expect to be reached as `common/packet.h`,
    # `d2dbs/dbserver.h`, etc -- i.e. relative to `src/`.
    target_include_directories(integration_legacy_d2dbs_linked PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR})
endif()

# ---------------------------------------------------------------------------
# 2. d2dbs / d2dbs_legacy strangler wiring
# ---------------------------------------------------------------------------
# Previously gated on `if(TARGET integration_legacy_d2dbs_linked)` in
# src/d2dbs/CMakeLists.txt -- which was always FALSE under the broken
# ordering. Here we are in the post-src subdir, so both `d2dbs` and
# `d2dbs_legacy` are guaranteed to exist, and `integration_legacy_d2dbs_linked`
# exists iff the if-block above fired.
if(TARGET d2dbs AND TARGET integration_legacy_d2dbs_linked)
    target_link_libraries(d2dbs PRIVATE integration_legacy_d2dbs_linked)
    target_compile_definitions(d2dbs PRIVATE PVPGN_V3_D2DBS_INTEGRATION=1)
    target_compile_definitions(d2dbs_legacy PRIVATE PVPGN_V3_D2DBS_INTEGRATION=1)
    target_include_directories(d2dbs_legacy PRIVATE
        ${CMAKE_SOURCE_DIR}/src/integration/legacy_d2dbs/include)
endif()
