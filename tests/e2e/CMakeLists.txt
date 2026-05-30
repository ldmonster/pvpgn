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
