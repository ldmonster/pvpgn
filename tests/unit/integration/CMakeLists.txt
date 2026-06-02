# legacy_bnetd integration tests were DELETED (Plan 10 legacy-linkage cleanup):
# they exercised the strangler bridges under src/integration/legacy_bnetd/,
# themselves removed in the Plan 03 finalization. The behaviour is now covered
# by the v3-native bnetd path; the d2cs/d2dbs strangler tests below were
# migrated to the app_d2cs/app_d2dbs targets and are kept.
add_subdirectory(legacy_d2cs)
add_subdirectory(legacy_d2dbs)
if(TARGET pvpgn_integration_wol)
    add_subdirectory(wol)
endif()
if(TARGET pvpgn_integration_telnet)
    add_subdirectory(telnet)
endif()
if(TARGET pvpgn_integration_bnet)
    add_subdirectory(bnet)
endif()
if(TARGET pvpgn_integration_irc)
    add_subdirectory(irc)
endif()

# SPDX-License-Identifier: GPL-2.0-or-later

pvpgn_v3_add_test(test_integration_bnet_session_flow
    SOURCES bnet_session_flow_test.cpp
    DEPS    application_chat
            application_game
            infra_inmemory
            infra_storage_repository)
