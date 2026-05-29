# R347 Checklist — seccomp Sandbox for Linux Plugins

## Goal
Wrap each plugin's `init` and event callbacks in a seccomp-BPF sandbox on Linux
that restricts syscalls to a safe allowlist.

## Files Created / Modified

- [x] `src/v3/infra/plugin/include/infra/plugin/sandbox.hpp`
  - `run_sandboxed(std::function<int()> fn)` — runs fn inside seccomp sandbox
  - `sandbox_available() noexcept` — returns true only on Linux + seccomp build
  - Full Doxygen documentation including syscall allowlist table
  - Strict-mode documentation (env var + compile-time macro)

- [x] `src/v3/infra/plugin/src/sandbox.cpp`
  - **Path 1** (Linux + `PVPGN_V3_SECCOMP_ENABLED`):
    - `seccomp_init(SCMP_ACT_KILL)` — default action kills process
    - Allowlist: `read`, `write`, `mmap`, `munmap`, `brk`, `exit`, `exit_group`,
      `futex`, `clock_gettime`, `gettimeofday`
    - `seccomp_load()` installs the filter
    - Strict mode: throws `std::runtime_error` on failure when env var set
    - Non-strict mode: falls back to unsandboxed execution on failure
  - **Path 2** (non-Linux or seccomp disabled):
    - `run_sandboxed()` calls `fn()` directly
    - `sandbox_available()` returns `false`

- [x] `src/v3/infra/plugin/CMakeLists.txt` (updated — already includes R347)
  - `option(PVPGN_V3_WITH_SECCOMP "..." OFF)`
  - `find_package(PkgConfig)` + `pkg_check_modules(LIBSECCOMP REQUIRED libseccomp)`
  - `PVPGN_V3_SECCOMP_ENABLED=1` compile definition when enabled
  - `sandbox.cpp` included in `pvpgn_infra_plugin` sources
  - `${LIBSECCOMP_LIBRARIES}` linked when enabled

- [x] `docs/sandbox-integration-guide.md` (updated)
  - How to enable seccomp: `-DPVPGN_V3_WITH_SECCOMP=ON`
  - Installation instructions for `libseccomp-dev`
  - `run_sandboxed()` usage example
  - `sandbox_available()` usage example
  - Syscall allowlist rationale table
  - How to extend the allowlist
  - Strict mode documentation
  - Non-Linux fallback behaviour
  - Comparison table: R347 lightweight vs legacy full sandbox
  - "Choosing the right sandbox layer" guidance

## Acceptance Criteria

- [x] `sandbox_available()` returns `false` when `PVPGN_V3_WITH_SECCOMP=OFF`
- [x] `run_sandboxed(fn)` calls `fn()` directly when seccomp not available
- [x] On Linux + seccomp: `SCMP_ACT_KILL` is the default action
- [x] Allowlist covers exactly the 10 documented syscalls
- [x] Strict mode throws `std::runtime_error` on seccomp failure
- [x] Non-strict mode falls back gracefully (no crash)
- [x] CMake option defaults to `OFF` (opt-in)
- [x] `pkg_check_modules(LIBSECCOMP REQUIRED ...)` used for dependency detection
- [x] Documentation covers all four topics: enable, allowlist, extend, fallback
