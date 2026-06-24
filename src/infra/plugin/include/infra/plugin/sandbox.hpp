// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/**
 * @file sandbox.hpp
 * @brief seccomp-BPF sandbox wrapper for native plugin callbacks.
 *
 * On Linux with `PVPGN_V3_WITH_SECCOMP=ON` the sandbox restricts the set of
 * syscalls available to a plugin's `init` and event-handler callbacks to a
 * minimal safe allowlist.  On all other platforms (or when seccomp is not
 * compiled in) the function runs without any OS-level restriction.
 *
 * ### Syscall allowlist (Linux + seccomp)
 * | Syscall         | Rationale                                      |
 * |-----------------|------------------------------------------------|
 * | read            | Read from already-open file descriptors        |
 * | write           | Write to already-open file descriptors         |
 * | mmap            | Heap allocation (malloc/new)                   |
 * | munmap          | Heap deallocation (free/delete)                |
 * | brk             | Heap growth                                    |
 * | exit            | Normal thread/process exit                     |
 * | exit_group      | Normal process exit                            |
 * | futex           | Mutex / condition variable operations          |
 * | clock_gettime   | Monotonic / wall-clock time queries            |
 * | gettimeofday    | Legacy time queries                            |
 *
 * Default action: `SCMP_ACT_KILL` — any other syscall terminates the process.
 *
 * ### Extending the allowlist
 * If a plugin legitimately needs additional syscalls, add them in
 * `src/v3/infra/plugin/src/sandbox.cpp` inside the `#ifdef PVPGN_V3_SECCOMP_ENABLED`
 * block and document the rationale.
 *
 * ### Strict mode
 * If the environment variable `PVPGN_PLUGIN_STRICT_SANDBOX=1` is set (or the
 * compile-time macro is defined), `run_sandboxed()` throws `std::runtime_error`
 * when seccomp setup fails instead of falling back to unsandboxed execution.
 */

#include <functional>

namespace pvpgn::infra::plugin {

/**
 * @brief Run @p fn inside a seccomp-BPF sandbox (Linux only).
 *
 * On Linux with seccomp compiled in:
 *   1. Installs the syscall allowlist via libseccomp.
 *   2. Calls `fn()`.
 *   3. Returns the return value of `fn()`.
 *
 * On non-Linux platforms or when `PVPGN_V3_WITH_SECCOMP=OFF`:
 *   - Calls `fn()` directly without any sandboxing.
 *
 * @param fn  Callable returning `int`.  Must not capture references to objects
 *            that may be destroyed before the call returns.
 * @return    The value returned by `fn()`.
 * @throws    `std::runtime_error` if sandbox setup fails **and**
 *            `PVPGN_PLUGIN_STRICT_SANDBOX` is defined or the environment
 *            variable `PVPGN_PLUGIN_STRICT_SANDBOX=1` is set.
 */
[[nodiscard]] int run_sandboxed(std::function<int()> fn);

/**
 * @brief Query whether seccomp sandboxing is available on this build/platform.
 *
 * @return `true`  if `PVPGN_V3_WITH_SECCOMP=ON` and the platform is Linux.
 * @return `false` otherwise.
 */
[[nodiscard]] bool sandbox_available() noexcept;

} // namespace pvpgn::infra::plugin
