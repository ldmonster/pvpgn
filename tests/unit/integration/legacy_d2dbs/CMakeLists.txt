pvpgn_v3_add_test(test_app_d2dbs_send_packet_bridge
    SOURCES send_packet_bridge_test.cpp
    DEPS    app_d2dbs)

pvpgn_v3_add_test(test_app_d2dbs_send_echorequest_bridge
    SOURCES send_echorequest_bridge_test.cpp
    DEPS    app_d2dbs)

# R231: observation lifecycle bridges (charlock, d2ladder).
pvpgn_v3_add_test(test_app_d2dbs_charlock_bridge
    SOURCES charlock_bridge_test.cpp
    DEPS    app_d2dbs)

pvpgn_v3_add_test(test_app_d2dbs_d2ladder_bridge
    SOURCES d2ladder_bridge_test.cpp
    DEPS    app_d2dbs)

# R232: observation bridges for dupecheck / dbserver / signals.
pvpgn_v3_add_test(test_app_d2dbs_dbsdupecheck_bridge
    SOURCES dbsdupecheck_bridge_test.cpp
    DEPS    app_d2dbs)

pvpgn_v3_add_test(test_app_d2dbs_dbserver_bridge
    SOURCES dbserver_bridge_test.cpp
    DEPS    app_d2dbs)

pvpgn_v3_add_test(test_app_d2dbs_handle_signal_bridge
    SOURCES handle_signal_bridge_test.cpp
    DEPS    app_d2dbs)

# R242: d2dbs packet dispatcher bridges (handle / timeout / keepalive).
pvpgn_v3_add_test(test_app_d2dbs_dbspacket_bridge
    SOURCES dbspacket_bridge_test.cpp
    DEPS    app_d2dbs)
