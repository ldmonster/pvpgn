if(TARGET infra_legacy_crypto)
    pvpgn_v3_add_test(test_infra_legacy_crypto_bnet_session_hasher
        SOURCES bnet_session_hasher_test.cpp
        DEPS    infra_legacy_crypto common)
    target_include_directories(
        test_infra_legacy_crypto_bnet_session_hasher PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR})
    target_compile_options(
        test_infra_legacy_crypto_bnet_session_hasher PRIVATE -w)
endif()
