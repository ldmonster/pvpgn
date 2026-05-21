pvpgn_v3_add_test(test_infra_compat_process
    SOURCES process_test.cpp
    DEPS    infra_compat)

pvpgn_v3_add_test(test_infra_compat_directory
    SOURCES test_directory.cpp
    DEPS    infra_compat)

pvpgn_v3_add_test(test_infra_compat_getopt
    SOURCES test_getopt.cpp
    DEPS    infra_compat)
