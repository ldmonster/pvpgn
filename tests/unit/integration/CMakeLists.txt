add_subdirectory(legacy_bnetd)

# SPDX-License-Identifier: GPL-2.0-or-later

pvpgn_v3_add_test(test_integration_bnet_session_flow
    SOURCES bnet_session_flow_test.cpp
    DEPS    application_chat
            application_game
            infra_inmemory
            infra_storage)
