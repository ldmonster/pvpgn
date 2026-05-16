pvpgn_v3_add_test(test_runtime_service_config
    SOURCES service_config_test.cpp
    DEPS    core runtime)

if(PVPGN_V3_WITH_BOOST)
    pvpgn_v3_add_test(test_runtime_peer_link
        SOURCES peer_link_test.cpp
        DEPS    core runtime OpenSSL::SSL OpenSSL::Crypto)
endif()
