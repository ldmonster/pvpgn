pvpgn_v3_add_test(test_infra_crypto_bnet_hash
    SOURCES bnet_hash_test.cpp
    DEPS    infra_crypto)

# Plan 08: SRP-3 golden-vector / round-trip regression test.
pvpgn_v3_add_test(test_infra_crypto_bnet_srp3_golden
    SOURCES bnet_srp3_golden_test.cpp
    DEPS    infra_crypto)

# Plan 10: SRP-3 mathematical-invariant property tests.
pvpgn_v3_add_test(test_infra_crypto_bnet_srp3_property
    SOURCES bnet_srp3_property_test.cpp
    DEPS    infra_crypto)

# The legacy-vs-v3 parity test was removed with the dead legacy crypto chain
# (Plan 08): the legacy oracle it compared against was never built and had been
# uncompilable since Plan 02. The v3 infra/crypto behaviour is covered by
# bnet_hash_test / bnet_srp3_golden_test / bnet_srp3_property_test above.

# NLS/SRP-6a tests (R281). Only built when OpenSSL is available since
# the implementation depends on OpenSSL::Crypto.
if(TARGET OpenSSL::Crypto)
    pvpgn_v3_add_test(test_infra_crypto_nls
        SOURCES nls_test.cpp
        DEPS    infra_crypto_nls OpenSSL::Crypto)
endif()
