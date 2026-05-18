pvpgn_v3_add_test(test_infra_crypto_bnet_hash
    SOURCES bnet_hash_test.cpp
    DEPS    infra_crypto)

if(TARGET common)
    pvpgn_v3_add_test(test_infra_crypto_parity
        SOURCES parity_test.cpp
        DEPS    infra_crypto common)
    target_include_directories(
        test_infra_crypto_parity PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR})
    # The legacy headers under common/ are full of comparison and
    # unused warnings; don't fail this test on them.
    target_compile_options(test_infra_crypto_parity PRIVATE -w)
endif()
