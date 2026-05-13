pvpgn_v3_add_test(test_infra_net_echo
    SOURCES echo_test.cpp
    DEPS    infra_net)

pvpgn_v3_add_test(test_infra_net_udp_echo
    SOURCES udp_echo_test.cpp
    DEPS    infra_net)

if(PVPGN_V3_WITH_FIBER)
    pvpgn_v3_add_test(test_infra_net_fiber_session
        SOURCES fiber_session_test.cpp
        DEPS    infra_net)
    pvpgn_v3_add_test(test_infra_net_fiber_pool
        SOURCES fiber_pool_test.cpp
        DEPS    infra_net)
endif()
