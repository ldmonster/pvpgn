#pragma once
#include "sandbox_policy.hpp"
#include "core/result.hpp"
#include "core/error.hpp"
#include <vector>

namespace pvpgn::infra::sandbox {

/// SeccompFilter — applies a seccomp-BPF syscall filter to the current thread/process.
///
/// Uses libseccomp (if available) or raw BPF (fallback) to restrict syscalls.
/// Must be called after fork() but before exec() or plugin load.
class SeccompFilter {
public:
    SeccompFilter() = default;
    ~SeccompFilter() = default;

    /// Apply the seccomp filter based on the policy's allowed_syscalls.
    /// After this call, any syscall not in the allowlist causes SIGSYS.
    /// Returns Error if seccomp is not supported or filter application fails.
    [[nodiscard]] static core::Result<void, core::Error>
    apply(const SandboxPolicy& policy);

    /// Check if seccomp is available on this system.
    [[nodiscard]] static bool is_available() noexcept;

    /// Get the list of syscall numbers for a given category.
    [[nodiscard]] static std::vector<int>
    syscalls_for_category(SyscallCategory category);
};

} // namespace pvpgn::infra::sandbox
