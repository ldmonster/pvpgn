# R339 — CMakePresets.json + Sanitizer Presets

## Checklist
- [x] Read existing `CMakePresets.json` (version 6, had `_base`, `dev-debug`, `dev-release`, `dev-asan`, `ci-coverage`, `legacy-release` presets)
- [x] Added `v3-dev` configure preset — Debug, `PVPGN_BUILD_V3=ON`, `PVPGN_BUILD_LEGACY=OFF`
- [x] Added `v3-release` configure preset — Release, `PVPGN_BUILD_V3=ON`, `PVPGN_BUILD_LEGACY=OFF`
- [x] Added `v3-asan` configure preset — Debug + `-fsanitize=address,undefined`
- [x] Added `v3-tsan` configure preset — Debug + `-fsanitize=thread`
- [x] Added `v3-coverage` configure preset — Debug + `--coverage -fprofile-arcs -ftest-coverage`
- [x] Added `v3-fuzz` configure preset — Debug + `-fsanitize=fuzzer,address`
- [x] Added corresponding build presets for all six new configure presets
- [x] Added test presets for `v3-dev`, `v3-asan`, `v3-tsan`, `v3-coverage` with `outputOnFailure: true`
- [x] Preserved all existing presets (`_base`, `dev-debug`, `dev-release`, `dev-asan`, `ci-coverage`, `legacy-release`)

## Result
`CMakePresets.json` now contains a complete set of standardised presets for all
build matrix axes: development, release, AddressSanitizer+UBSan, ThreadSanitizer,
coverage instrumentation, and fuzzing. The new `v3-*` presets are standalone
(no `inherits`) to match the spec. All existing presets are preserved unchanged.
