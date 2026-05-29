#include "infra/sandbox/apparmor_confinement.hpp"
#include "core/error.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>

#ifdef HAVE_APPARMOR
#include <sys/apparmor.h>
#endif

namespace pvpgn::infra::sandbox {

bool AppArmorConfinement::is_available() noexcept {
#ifdef HAVE_APPARMOR
    // Check if AppArmor profiles directory exists
    std::filesystem::path profiles_path{"/sys/kernel/security/apparmor/profiles"};
    return std::filesystem::exists(profiles_path);
#else
    return false;
#endif
}

core::Result<std::string, core::Error> AppArmorConfinement::current_label() {
    try {
        std::ifstream attr_file{"/proc/self/attr/current"};
        if (!attr_file.is_open()) {
            return core::fail(core::make_error(
                core::StatusCode::Unavailable,
                "cannot read /proc/self/attr/current"));
        }

        std::string label;
        std::getline(attr_file, label);

        // Trim trailing whitespace
        while (!label.empty() && std::isspace(label.back())) {
            label.pop_back();
        }

        return label;
    } catch (const std::exception& e) {
        return core::fail(core::make_error(
            core::StatusCode::Internal,
            std::string("failed to read AppArmor label: ") + e.what()));
    }
}

std::string AppArmorConfinement::generate_profile(const SandboxPolicy& policy) {
    std::ostringstream profile;

    // Generate profile name
    std::string profile_name = policy.apparmor_profile;
    if (profile_name.empty()) {
        profile_name = "pvpgn-plugin-" + policy.plugin_name;
    }

    // Build the profile
    profile << "profile " << profile_name << " flags=(attach_disconnected) {\n";
    profile << "  # Deny everything by default\n";
    profile << "  deny /** rwklmx,\n";
    profile << "\n";

    // Allow read-only access to specified paths
    if (!policy.allowed_paths.empty()) {
        profile << "  # Allow read-only access to specified paths\n";
        for (const auto& path : policy.allowed_paths) {
            profile << "  " << path << "/** r,\n";
        }
        profile << "\n";
    }

    // Allow basic capabilities
    profile << "  # Allow basic capabilities\n";
    profile << "  capability setuid,\n";
    profile << "  network inet stream,\n";
    profile << "}\n";

    return profile.str();
}

core::Result<void, core::Error> AppArmorConfinement::confine(const SandboxPolicy& policy) {
    // If AppArmor is disabled, return success
    if (policy.apparmor_level == AppArmorLevel::Disabled) {
        return core::Result<void, core::Error>();
    }

    // Check if AppArmor is available
    if (!is_available()) {
        return core::fail(core::make_error(
            core::StatusCode::Unavailable,
            "AppArmor is not available on this system"));
    }

#ifdef HAVE_APPARMOR
    // Generate profile name
    std::string profile_name = policy.apparmor_profile;
    if (profile_name.empty()) {
        profile_name = "pvpgn-plugin-" + policy.plugin_name;
    }

    // Determine the mode string
    const char* mode_str = "enforce";
    if (policy.apparmor_level == AppArmorLevel::Complain) {
        mode_str = "complain";
    }

    // Try to transition to the profile
    // aa_change_profile() takes profile name and mode
    if (aa_change_profile(profile_name.c_str()) < 0) {
        return core::fail(core::make_error(
            core::StatusCode::Internal,
            std::string("failed to transition to AppArmor profile: ") + profile_name));
    }

    return core::Result<void, core::Error>();
#else
    // AppArmor support not compiled in
    return core::fail(core::make_error(
        core::StatusCode::Unimplemented,
        "AppArmor support not compiled in"));
#endif
}

} // namespace pvpgn::infra::sandbox
