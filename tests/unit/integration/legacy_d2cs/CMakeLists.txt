pvpgn_v3_add_test(test_integration_legacy_d2cs_send_packet_bridge
    SOURCES send_packet_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_send_loginreply_bridge
    SOURCES send_loginreply_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_send_createcharreply_bridge
    SOURCES send_createcharreply_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_send_charloginreply_bridge
    SOURCES send_charloginreply_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_send_gamereply_bridges
    SOURCES send_gamereply_bridges_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_send_handle_bnetd_bridges
    SOURCES send_handle_bnetd_bridges_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_send_handle_d2gs_bridges
    SOURCES send_handle_d2gs_bridges_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_send_simple_replies
    SOURCES send_simple_replies_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_send_listreply_bridges
    SOURCES send_listreply_bridges_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_send_ladderreply_bridge
    SOURCES send_d2cs_listreply_encoders_bridges_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_send_outbound_obs_bridges
    SOURCES send_outbound_obs_bridges_test.cpp
    DEPS    integration_legacy_d2cs)

# R233: d2cs lifecycle observation bridges.
pvpgn_v3_add_test(test_integration_legacy_d2cs_d2ladder_bridge
    SOURCES d2ladder_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_s2s_bridge
    SOURCES s2s_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_handle_signal_bridge
    SOURCES handle_signal_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

# R234: more d2cs observation bridges (server loop + init + conn).
pvpgn_v3_add_test(test_integration_legacy_d2cs_server_bridge
    SOURCES server_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_handle_init_bridge
    SOURCES handle_init_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_conn_bridge
    SOURCES conn_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

# R235: d2cs packet-dispatcher observation bridges.
pvpgn_v3_add_test(test_integration_legacy_d2cs_handle_d2cs_packet_bridge
    SOURCES handle_d2cs_packet_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_handle_d2gs_packet_bridge
    SOURCES handle_d2gs_packet_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_handle_bnetd_packet_bridge
    SOURCES handle_bnetd_packet_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

# R236: d2cs queue + d2gs-list lifecycle bridges.
pvpgn_v3_add_test(test_integration_legacy_d2cs_gamequeue_bridge
    SOURCES gamequeue_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_serverqueue_bridge
    SOURCES serverqueue_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_d2gs_bridge
    SOURCES d2gs_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

# R238: d2cs game catalogue + per-game lifecycle bridges.
pvpgn_v3_add_test(test_integration_legacy_d2cs_game_bridge
    SOURCES game_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

# R241: d2cs net.cpp socket helper bridges.
pvpgn_v3_add_test(test_integration_legacy_d2cs_net_bridge
    SOURCES net_bridge_test.cpp
    DEPS    integration_legacy_d2cs)
