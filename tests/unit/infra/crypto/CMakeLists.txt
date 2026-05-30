pvpgn_v3_add_test(test_infra_crypto_bnet_hash
    SOURCES bnet_hash_test.cpp
    DEPS    infra_crypto)

# Parity test compares the new infra/crypto implementation against the
# legacy src/common/ code. Guard on target existence since PVPGN_BUILD_LEGACY
# has been removed; the `common` target is present when the legacy tree is built.
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

# NLS/SRP-6a tests (R281). Only built when OpenSSL is available since
# the implementation depends on OpenSSL::Crypto.
if(TARGET OpenSSL::Crypto)
    pvpgn_v3_add_test(test_infra_crypto_nls
        SOURCES nls_test.cpp
        DEPS    infra_crypto_nls OpenSSL::Crypto)
endif()
