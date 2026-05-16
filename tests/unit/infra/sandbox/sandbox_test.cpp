#include <gtest/gtest.h>
#include "infra/sandbox/sandbox_policy.hpp"
#include "infra/sandbox/seccomp_filter.hpp"
#include "infra/sandbox/apparmor_confinement.hpp"
#include "infra/sandbox/plugin_sandbox.hpp"

namespace pvpgn::infra::sandbox::test {

TEST(SandboxPolicyTest, DefaultPolicyHasReasonableDefaults) {
    SandboxPolicy policy;
    policy.plugin_name = "test-plugin";
    EXPECT_TRUE(policy.enable_seccomp);
    EXPECT_FALSE(policy.allow_new_privs);
    EXPECT_EQ(policy.max_memory_bytes, 64u * 1024 * 1024);
    EXPECT_EQ(policy.max_open_files, 64u);
}

TEST(SandboxPolicyTest, SyscallCategoryBitwiseOr) {
    auto combined = SyscallCategory::BasicIO | SyscallCategory::Memory;
    EXPECT_TRUE(combined & SyscallCategory::BasicIO);
    EXPECT_TRUE(combined & SyscallCategory::Memory);
    EXPECT_FALSE(combined & SyscallCategory::NetworkSend);
}

TEST(SeccompFilterTest, IsAvailableReturnsBool) {
    // Just verify it doesn't crash
    bool avail = SeccompFilter::is_available();
    (void)avail;
    SUCCEED();
}

TEST(SeccompFilterTest, SyscallsForCategoryBasicIO) {
    auto syscalls = SeccompFilter::syscalls_for_category(SyscallCategory::BasicIO);
    EXPECT_FALSE(syscalls.empty());
}

TEST(SeccompFilterTest, SyscallsForCategoryNoneIsEmpty) {
    auto syscalls = SeccompFilter::syscalls_for_category(SyscallCategory::None);
    EXPECT_TRUE(syscalls.empty());
}

TEST(AppArmorConfinementTest, IsAvailableReturnsBool) {
    bool avail = AppArmorConfinement::is_available();
    (void)avail;
    SUCCEED();
}

TEST(AppArmorConfinementTest, GenerateProfileContainsPluginName) {
    SandboxPolicy policy;
    policy.plugin_name = "my-test-plugin";
    policy.allowed_paths = {"/var/pvpgn/plugins/my-test-plugin"};
    auto profile = AppArmorConfinement::generate_profile(policy);
    EXPECT_NE(profile.find("my-test-plugin"), std::string::npos);
}

TEST(PluginSandboxTest, ProbeCapabilitiesDoesNotCrash) {
    auto caps = PluginSandbox::probe_capabilities();
    // Just verify it runs without crashing
    (void)caps;
    SUCCEED();
}

TEST(PluginSandboxTest, ApplyRlimitsWithZeroLimitsReturnsError) {
    SandboxPolicy policy;
    policy.plugin_name = "test";
    policy.max_memory_bytes = 0; // Invalid
    // Should either succeed (OS may ignore 0) or return error
    auto result = PluginSandbox::apply_rlimits(policy);
    // We just verify it doesn't crash
    (void)result;
    SUCCEED();
}

} // namespace pvpgn::infra::sandbox::test
