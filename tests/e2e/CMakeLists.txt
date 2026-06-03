# SPDX-License-Identifier: GPL-2.0-or-later
# E2E tests — drive real bnetd via real client tools (bnbot, bnchat, bnftp, bnstat)
#
# These tests are driven by the shell scripts in scripts/:
#   scripts/v3-e2e-bnbot-smoke.sh
#   scripts/v3-e2e-bnchat-smoke.sh
#   scripts/v3-e2e-bnftp-smoke.sh
#   scripts/v3-e2e-bnstat-smoke.sh
#
# They are registered as CTest tests so they appear in ctest output.
# Requires: PVPGN_V3_E2E_TESTS=ON and a running bnetd instance.

find_program(BASH_EXECUTABLE bash REQUIRED)

set(E2E_SCRIPTS
    "${PROJECT_SOURCE_DIR}/scripts/v3-e2e-bnbot-smoke.sh"
    "${PROJECT_SOURCE_DIR}/scripts/v3-e2e-bnchat-smoke.sh"
    "${PROJECT_SOURCE_DIR}/scripts/v3-e2e-bnftp-smoke.sh"
    "${PROJECT_SOURCE_DIR}/scripts/v3-e2e-bnstat-smoke.sh"
)

foreach(script ${E2E_SCRIPTS})
    get_filename_component(test_name "${script}" NAME_WE)
    add_test(
        NAME "e2e.${test_name}"
        COMMAND "${BASH_EXECUTABLE}" "${script}"
    )
    set_tests_properties("e2e.${test_name}" PROPERTIES
        TIMEOUT 120
        LABELS "e2e"
    )
endforeach()

# --- modern-flow login journey: Python client vs a *real* bnetd -----------
#
# The *-smoke.sh tests above drive real client tools against a Python *mock*
# server. This test is the inverse: a stdlib-only Python client spawns the
# real `bnetd` (inmemory backend, ephemeral bnet port) and drives the modern
# SID auth handshake (AUTH_INFO -> AUTH_CHECK -> LOGONRESPONSE2). It is the
# only test that exercises real bnetd's wire dispatch / session-context send /
# connection teardown paths end to end -- it is what surfaced and now guards
# the dispatch use-after-free, the double-finalize send bug, and the on_close
# dangling-`this` crash (see docs/refactoring/progress.md, M1 Step 1.1).
#
# RUN_SERIAL: bnetd still binds its (currently non-configurable) auxiliary
# ports 6112/4000/6667 in addition to the ephemeral bnet port, so two copies
# must not run concurrently. A port collision makes bnetd exit at startup,
# which the harness reports as a clear failure (not a hang).
find_program(PYTHON3_EXECUTABLE NAMES python3 python)
if(PYTHON3_EXECUTABLE AND TARGET bnetd)
    add_test(
        NAME "e2e.modern_login_journey"
        COMMAND "${PYTHON3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/modern_login_journey_test.py"
            --bnetd "$<TARGET_FILE:bnetd>"
    )
    set_tests_properties("e2e.modern_login_journey" PROPERTIES
        TIMEOUT 60
        RUN_SERIAL TRUE
        LABELS "e2e"
    )
else()
    message(STATUS
        "tests/e2e: skipping modern_login_journey "
        "(need python3 + the bnetd target)")
endif()
