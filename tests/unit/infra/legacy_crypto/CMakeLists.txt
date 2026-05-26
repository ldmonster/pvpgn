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

# Ported from the legacy `src/test/{bigint,bnetsrp3_test}.cpp`
# standalone binaries (see plans/refactoring-plan-testing.md "Legacy
# Test Removal"). These exercise the legacy `common` library directly
# and do not need any v3 infra target; they are gated only on the
# `common` legacy target being present.
if(PVPGN_BUILD_LEGACY)
    pvpgn_v3_add_test(test_common_bigint
        SOURCES bigint_test.cpp
        DEPS    common)
    target_include_directories(test_common_bigint PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR})
    target_compile_options(test_common_bigint PRIVATE -w)

    pvpgn_v3_add_test(test_common_bnetsrp3
        SOURCES bnetsrp3_test.cpp
        DEPS    common)
    target_include_directories(test_common_bnetsrp3 PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR})
    target_compile_options(test_common_bnetsrp3 PRIVATE -w)
endif()
