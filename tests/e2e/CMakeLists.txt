# End-to-End tests for v3 sub-tree

pvpgn_v3_add_test(test_e2e_service_startup
    SOURCES service_startup_test.cpp
    DEPS    core runtime)
