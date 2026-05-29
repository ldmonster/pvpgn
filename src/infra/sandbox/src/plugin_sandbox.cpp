#include "infra/sandbox/plugin_sandbox.hpp"
#include "core/error.hpp"

#ifdef __linux__
#include <sys/resource.h>
#endif

namespace pvpgn::infra::sandbox {

PluginSandbox::Capabilities PluginSandbox::probe_capabilities() noexcept {
    return Capabilities{
        .seccomp_available = SeccompFilter::is_available(),
        .apparmor_available = AppArmorConfinement::is_available(),
        .rlimits_available = true,  // rlimits are always available on POSIX systems
    };
}

core::Result<void, core::Error> PluginSandbox::apply_rlimits(const SandboxPolicy& policy) {
#ifndef __linux__
    // rlimits are POSIX, but we'll gracefully skip on non-Linux
    return core::Result<void, core::Error>();
#else
    // Set memory limit (RLIMIT_AS)
    if (policy.max_memory_bytes > 0) {
        struct rlimit mem_limit;
        mem_limit.rlim_cur = policy.max_memory_bytes;
        mem_limit.rlim_max = policy.max_memory_bytes;
        if (setrlimit(RLIMIT_AS, &mem_limit) < 0) {
            return core::fail(core::make_error(
                core::StatusCode::Internal,
                "failed to set RLIMIT_AS"));
        }
    }

    // Set open files limit (RLIMIT_NOFILE)
    if (policy.max_open_files > 0) {
        struct rlimit file_limit;
        file_limit.rlim_cur = policy.max_open_files;
        file_limit.rlim_max = policy.max_open_files;
        if (setrlimit(RLIMIT_NOFILE, &file_limit) < 0) {
            return core::fail(core::make_error(
                core::StatusCode::Internal,
                "failed to set RLIMIT_NOFILE"));
        }
    }

    // Prevent spawning new processes (RLIMIT_NPROC = 0)
    struct rlimit proc_limit;
    proc_limit.rlim_cur = 0;
    proc_limit.rlim_max = 0;
    if (setrlimit(RLIMIT_NPROC, &proc_limit) < 0) {
        return core::fail(core::make_error(
            core::StatusCode::Internal,
            "failed to set RLIMIT_NPROC"));
    }

    return core::Result<void, core::Error>();
#endif
}

core::Result<void, core::Error> PluginSandbox::apply(const SandboxPolicy& policy) {
    // Apply resource limits first
    auto rlimit_result = apply_rlimits(policy);
    if (!rlimit_result) {
        return rlimit_result;
    }

    // Apply seccomp filter if enabled
    if (policy.enable_seccomp) {
        auto seccomp_result = SeccompFilter::apply(policy);
        if (!seccomp_result) {
            return seccomp_result;
        }
    }

    // Apply AppArmor confinement if enabled
    if (policy.apparmor_level != AppArmorLevel::Disabled) {
        auto apparmor_result = AppArmorConfinement::confine(policy);
        if (!apparmor_result) {
            return apparmor_result;
        }
    }

    return core::Result<void, core::Error>();
}

} // namespace pvpgn::infra::sandbox
