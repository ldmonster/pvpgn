add_subdirectory(legacy_bnetd)
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
