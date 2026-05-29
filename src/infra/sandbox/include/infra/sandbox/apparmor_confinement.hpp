#pragma once
#include "sandbox_policy.hpp"
#include "core/result.hpp"
#include "core/error.hpp"
#include <string>

namespace pvpgn::infra::sandbox {

/// AppArmorConfinement — transitions the current process into an AppArmor profile.
///
/// Requires AppArmor kernel support and the aa_change_profile() function
/// from libapparmor. Falls back gracefully if AppArmor is not available.
class AppArmorConfinement {
public:
    AppArmorConfinement() = default;
    ~AppArmorConfinement() = default;

    /// Transition into the AppArmor profile specified in the policy.
    /// If policy.apparmor_profile is empty, generates a profile name from plugin_name.
    /// Returns Error if AppArmor is unavailable or transition fails.
    [[nodiscard]] static core::Result<void, core::Error>
    confine(const SandboxPolicy& policy);

    /// Generate an AppArmor profile string for a plugin.
    /// The profile restricts file access to policy.allowed_paths.
    [[nodiscard]] static std::string
    generate_profile(const SandboxPolicy& policy);

    /// Check if AppArmor is available and enabled on this system.
    [[nodiscard]] static bool is_available() noexcept;

    /// Get the current AppArmor confinement label for this process.
    [[nodiscard]] static core::Result<std::string, core::Error>
    current_label();
};

} // namespace pvpgn::infra::sandbox
