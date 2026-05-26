pvpgn_v3_add_test(test_infra_crypto_bnet_hash
    SOURCES bnet_hash_test.cpp
    DEPS    infra_crypto)

# Parity test compares the new infra/crypto implementation against the
# legacy src/common/ code. It is only buildable when the legacy tree is
# also compiled (PVPGN_BUILD_LEGACY=ON), so guard on that option rather
# than `if(TARGET common)` — the `common` target is registered AFTER the
# v3 sub-tree (which owns `tests/unit/`) finishes configuring, so the
# target-existence check is racy across CMake reorders.
if(PVPGN_BUILD_LEGACY)
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
