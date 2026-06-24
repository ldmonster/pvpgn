// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file sandbox.cpp
 * @brief seccomp-BPF sandbox implementation.
 *
 * Two compilation paths:
 *
 *  1. Linux + PVPGN_V3_SECCOMP_ENABLED (set by CMake when PVPGN_V3_WITH_SECCOMP=ON):
 *     Uses libseccomp to install a syscall allowlist before calling fn().
 *     Default action is SCMP_ACT_KILL — any unlisted syscall kills the process.
 *
 *  2. All other platforms / seccomp disabled:
 *     run_sandboxed() calls fn() directly; sandbox_available() returns false.
 */

#include "infra/plugin/sandbox.hpp"

#include <cstdlib>   // std::getenv
#include <functional>
#include <stdexcept>
#include <string>

// ============================================================================
// Path 1 — Linux + libseccomp
// ============================================================================
#if defined(PVPGN_V3_SECCOMP_ENABLED) && defined(__linux__)

#include <seccomp.h>
#include <sys/syscall.h>

namespace pvpgn::infra::plugin {

namespace {

/// Returns true if strict-sandbox mode is requested (env var or compile flag).
bool strict_mode() noexcept {
#ifdef PVPGN_PLUGIN_STRICT_SANDBOX
    return true;
#else
    const char* v = std::getenv("PVPGN_PLUGIN_STRICT_SANDBOX");
    return v && (v[0] == '1');
#endif
}

/**
 * @brief Install the seccomp allowlist for the current thread/process.
 *
 * Allowed syscalls (see sandbox.hpp for rationale):
 *   read, write, mmap, munmap, brk, exit, exit_group,
 *   futex, clock_gettime, gettimeofday
 *
 * @throws std::runtime_error on failure when strict_mode() is true.
 * @return true on success, false on failure (non-strict mode).
 */
bool install_seccomp_filter() {
    // Default action: kill the process on any unlisted syscall.
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_KILL);
    if (!ctx) {
        const std::string msg = "sandbox: seccomp_init() failed";
        if (strict_mode()) throw std::runtime_error(msg);
        return false;
    }

    // Helper lambda: add a rule; on failure release ctx and throw/return.
    auto allow = [&](int syscall_nr) -> bool {
        int rc = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, syscall_nr, 0);
        if (rc != 0) {
            seccomp_release(ctx);
            const std::string msg = "sandbox: seccomp_rule_add() failed for syscall " +
                                    std::to_string(syscall_nr);
            if (strict_mode()) throw std::runtime_error(msg);
            return false;
        }
        return true;
    };

    // ---- Allowlist --------------------------------------------------------
    if (!allow(SCMP_SYS(read)))          return false;
    if (!allow(SCMP_SYS(write)))         return false;
    if (!allow(SCMP_SYS(mmap)))          return false;
    if (!allow(SCMP_SYS(munmap)))        return false;
    if (!allow(SCMP_SYS(brk)))           return false;
    if (!allow(SCMP_SYS(exit)))          return false;
    if (!allow(SCMP_SYS(exit_group)))    return false;
    if (!allow(SCMP_SYS(futex)))         return false;
    if (!allow(SCMP_SYS(clock_gettime))) return false;
    if (!allow(SCMP_SYS(gettimeofday)))  return false;
    // -----------------------------------------------------------------------

    int rc = seccomp_load(ctx);
    seccomp_release(ctx);

    if (rc != 0) {
        const std::string msg = "sandbox: seccomp_load() failed (rc=" + std::to_string(rc) + ")";
        if (strict_mode()) throw std::runtime_error(msg);
        return false;
    }

    return true;
}

} // anonymous namespace

int run_sandboxed(std::function<int()> fn) {
    install_seccomp_filter(); // may throw in strict mode; otherwise best-effort
    return fn();
}

bool sandbox_available() noexcept {
    return true;
}

} // namespace pvpgn::infra::plugin

// ============================================================================
// Path 2 — Non-Linux or seccomp disabled: no-op fallback
// ============================================================================
#else

namespace pvpgn::infra::plugin {

int run_sandboxed(std::function<int()> fn) {
    // No sandboxing available on this platform/build; run directly.
    return fn();
}

bool sandbox_available() noexcept {
    return false;
}

} // namespace pvpgn::infra::plugin

#endif // PVPGN_V3_SECCOMP_ENABLED && __linux__
