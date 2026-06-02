pvpgn_v3_add_test(test_infra_net_socket
    SOURCES test_socket.cpp
    DEPS    infra_net)

pvpgn_v3_add_test(test_infra_net_echo
    SOURCES echo_test.cpp
    DEPS    infra_net)

pvpgn_v3_add_test(test_infra_net_tcp_acceptor_adopt
    SOURCES tcp_acceptor_adopt_test.cpp
    DEPS    infra_net)

pvpgn_v3_add_test(test_infra_net_listener_parity
    SOURCES listener_parity_test.cpp
    DEPS    infra_net)

pvpgn_v3_add_test(test_infra_net_udp_echo
    SOURCES udp_echo_test.cpp
    DEPS    infra_net)

# Plan 06: idle-connection memory-footprint regression gate (deterministic
# sizeof budgets; the fiber-path case is compiled only with PVPGN_V3_WITH_FIBER).
pvpgn_v3_add_test(test_infra_net_idle_memory_footprint
    SOURCES idle_memory_footprint_test.cpp
    DEPS    infra_net)

if(PVPGN_V3_WITH_FIBER)
    pvpgn_v3_add_test(test_infra_net_fiber_session
        SOURCES fiber_session_test.cpp
        DEPS    infra_net)
    pvpgn_v3_add_test(test_infra_net_fiber_pool
        SOURCES fiber_pool_test.cpp
        DEPS    infra_net)
endif()
