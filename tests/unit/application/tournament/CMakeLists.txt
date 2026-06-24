pvpgn_v3_add_test(test_application_tournament
    SOURCES tournament_reply_test.cpp
    DEPS    application_tournament)

pvpgn_v3_add_test(test_application_tournament_edges
    SOURCES tournament_reply_edges_test.cpp
    DEPS    application_tournament)
