#pragma once
#include "sandbox_policy.hpp"
#include "seccomp_filter.hpp"
#include "apparmor_confinement.hpp"
#include "core/result.hpp"
#include "core/error.hpp"

namespace pvpgn::infra::sandbox {

/// PluginSandbox — high-level facade that applies all sandbox layers.
///
/// Usage:
///   SandboxPolicy policy;
///   policy.plugin_name = "my-plugin";
///   policy.enable_seccomp = true;
///   policy.apparmor_level = AppArmorLevel::Enforce;
///
///   auto result = PluginSandbox::apply(policy);
///   if (!result) { /* handle error */ }
///   // Now load and run the plugin
class PluginSandbox {
public:
    /// Apply all sandbox layers specified in the policy.
    /// Order: rlimits → seccomp → AppArmor → no_new_privs
    /// Returns the first error encountered, or success.
    [[nodiscard]] static core::Result<void, core::Error>
    apply(const SandboxPolicy& policy);

    /// Apply only resource limits (rlimits) from the policy.
    [[nodiscard]] static core::Result<void, core::Error>
    apply_rlimits(const SandboxPolicy& policy);

    /// Check which sandbox features are available on this system.
    struct Capabilities {
        bool seccomp_available;
        bool apparmor_available;
        bool rlimits_available;
    };
    [[nodiscard]] static Capabilities probe_capabilities() noexcept;
};

} // namespace pvpgn::infra::sandbox
