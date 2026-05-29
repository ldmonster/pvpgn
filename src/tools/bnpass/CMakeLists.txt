# PvPGN v3 bnpass tool -- password hash generator.
#
# Replaces the legacy `src/bnpass/{bnpass,sha1hash}.cpp` standalone
# binaries (see plans/refactoring-plan-legacy-tools.md "Step 2").
# Hashing routes through `infra_crypto`, the v3 C++20 reimplementation
# of legacy `common/bnethash`; no legacy `common` / `compat` linkage.
#
# Single executable `pvpgn-bnpass` covers both legacy modes. By
# default it emits the `passhash1` attribute line; `--sha1` switches
# to a true-SHA-1 digest (replacing the legacy `sha1hash` CLI).

add_executable(pvpgn-bnpass src/main.cpp)

pvpgn_v3_apply_flags(pvpgn-bnpass)

target_link_libraries(pvpgn-bnpass PRIVATE infra_crypto)

# Pick up `PVPGN_VERSION` (defined by the top-level legacy build via
# `config.h`). When building v3-only with `PVPGN_BUILD_LEGACY=OFF`
# the include directory still exists and just lacks the macro, so we
# fall back to "unknown" inside main.cpp.
target_include_directories(pvpgn-bnpass PRIVATE ${CMAKE_BINARY_DIR})
target_compile_definitions(pvpgn-bnpass PRIVATE
    $<$<BOOL:${PVPGN_VERSION}>:PVPGN_VERSION="${PVPGN_VERSION}">)

install(TARGETS pvpgn-bnpass RUNTIME DESTINATION ${BIN_INSTALL_DIR})
