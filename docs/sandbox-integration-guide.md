# Plugin Sandboxing Integration Guide

## Overview

PvPGN-PRO v3 provides a comprehensive sandboxing infrastructure for plugin isolation using Linux seccomp and AppArmor. This defense-in-depth approach ensures that even if a malicious plugin escapes the Lua sandbox, the OS-level sandbox prevents damage to the host system.

## Architecture

The sandboxing system consists of three layers:

1. **Resource Limits (rlimits)** — Restrict memory, file descriptors, and process spawning
2. **Seccomp-BPF Filtering** — Restrict syscalls at the kernel level
3. **AppArmor Confinement** — Restrict file access and capabilities

## Quick Start

### Basic Plugin Sandboxing

```cpp
#include "infra/sandbox/plugin_sandbox.hpp"

using namespace pvpgn::infra::sandbox;

// Create a policy for your plugin
SandboxPolicy policy;
policy.plugin_name = "my-plugin";
policy.enable_seccomp = true;
policy.apparmor_level = AppArmorLevel::Enforce;
policy.allowed_paths = {"/var/pvpgn/plugins/my-plugin"};

// Apply the sandbox
auto result = PluginSandbox::apply(policy);
if (!result) {
    // Handle error
    auto error = std::move(result).error();
    std::cerr << "Sandbox failed: " << error.message() << std::endl;
    return;
}

// Now load and run the plugin
// ...
```

## Syscall Categories

The `SyscallCategory` enum defines allowlists for different types of plugins:

### Category Definitions

| Category | Syscalls | Use Case |
|----------|----------|----------|
| `BasicIO` | read, write, close, fstat, lseek | Basic I/O operations |
| `Memory` | mmap, mprotect, munmap, brk | Memory management |
| `Time` | clock_gettime, gettimeofday, nanosleep | Time queries and delays |
| `Process` | getpid, exit, exit_group | Process lifecycle |
| `FileRead` | open, openat, stat, access | Read-only file access |
| `FileWrite` | creat, unlink, rename | File creation/deletion |
| `NetworkRecv` | recv, recvfrom, recvmsg | Inbound network I/O |
| `NetworkSend` | send, sendto, sendmsg | Outbound network I/O |
| `Threading` | clone, futex, set_robust_list | Thread operations |
| `Signals` | rt_sigaction, rt_sigprocmask | Signal handling |

### Preset Combinations

- **`Lua`** = BasicIO | Memory | Time | Process
  - Minimal set for Lua scripts
  - No file or network access

- **`Plugin`** = Lua | FileRead | Signals
  - Standard plugin set
  - Read-only file access, signal handling

- **`Trusted`** = 0xFFFFFFFF
  - All syscalls allowed (no restriction)

## Policy Configuration

### SandboxPolicy Structure

```cpp
struct SandboxPolicy {
    std::string plugin_name;                    // For logging/profile naming
    SyscallCategory allowed_syscalls{SyscallCategory::Plugin};
    AppArmorLevel apparmor_level{AppArmorLevel::Disabled};
    std::string apparmor_profile;               // Empty = auto-generate
    bool enable_seccomp{true};                  // Apply seccomp BPF filter
    bool allow_new_privs{false};                // PR_SET_NO_NEW_PRIVS
    uint64_t max_memory_bytes{64 * 1024 * 1024}; // 64 MiB default
    uint32_t max_open_files{64};                // rlimit NOFILE
    std::vector<std::string> allowed_paths;     // Paths plugin may read
};
```

### Example Policies

#### Read-Only Plugin

```cpp
SandboxPolicy policy;
policy.plugin_name = "stats-reader";
policy.allowed_syscalls = SyscallCategory::Lua | SyscallCategory::FileRead;
policy.enable_seccomp = true;
policy.apparmor_level = AppArmorLevel::Enforce;
policy.allowed_paths = {"/var/pvpgn/stats", "/var/pvpgn/data"};
policy.max_memory_bytes = 32 * 1024 * 1024;  // 32 MiB
policy.max_open_files = 16;
```

#### Network-Capable Plugin

```cpp
SandboxPolicy policy;
policy.plugin_name = "network-plugin";
policy.allowed_syscalls = SyscallCategory::Plugin | SyscallCategory::NetworkRecv | SyscallCategory::NetworkSend;
policy.enable_seccomp = true;
policy.apparmor_level = AppArmorLevel::Complain;  // Log violations, don't enforce
policy.allowed_paths = {"/var/pvpgn/plugins/network-plugin"};
policy.max_memory_bytes = 128 * 1024 * 1024;  // 128 MiB
policy.max_open_files = 64;
```

#### Minimal Lua Script

```cpp
SandboxPolicy policy;
policy.plugin_name = "simple-script";
policy.allowed_syscalls = SyscallCategory::Lua;
policy.enable_seccomp = true;
policy.apparmor_level = AppArmorLevel::Disabled;  // No file access needed
policy.max_memory_bytes = 16 * 1024 * 1024;  // 16 MiB
policy.max_open_files = 4;
```

## AppArmor Levels

### AppArmorLevel Enum

- **`Disabled`** — No AppArmor confinement (default)
- **`Complain`** — Log violations but don't enforce (audit mode)
- **`Enforce`** — Enforce and deny violations (production mode)

### Profile Generation

AppArmor profiles are automatically generated from the policy:

```cpp
auto profile_text = AppArmorConfinement::generate_profile(policy);
// Generates a profile like:
// profile pvpgn-plugin-my-plugin flags=(attach_disconnected) {
//   deny /** rwklmx,
//   /var/pvpgn/plugins/my-plugin/** r,
//   capability setuid,
//   network inet stream,
// }
```

## Capability Detection

Check which sandbox features are available on the current system:

```cpp
auto caps = PluginSandbox::probe_capabilities();
if (caps.seccomp_available) {
    std::cout << "Seccomp is available" << std::endl;
}
if (caps.apparmor_available) {
    std::cout << "AppArmor is available" << std::endl;
}
if (caps.rlimits_available) {
    std::cout << "Resource limits are available" << std::endl;
}
```

## Build Configuration

### CMake Options

The sandbox module is built by default on Linux systems. To explicitly enable or disable:

```bash
cmake -DPVPGN_ENABLE_SANDBOX=ON ..
```

### AppArmor Support

AppArmor support is optional and detected automatically:

```bash
# Install AppArmor development libraries (Ubuntu/Debian)
sudo apt-get install libapparmor-dev

# Install AppArmor development libraries (Fedora/RHEL)
sudo dnf install apparmor-devel
```

If AppArmor is not available, the sandbox will gracefully fall back to seccomp-only mode.

## Error Handling

All sandbox operations return `Result<void, core::Error>`:

```cpp
auto result = PluginSandbox::apply(policy);
if (!result) {
    const auto& error = result.error();
    std::cerr << "Error: " << error.message() << std::endl;
    
    // Check error code
    switch (error.code()) {
        case core::StatusCode::Unavailable:
            std::cerr << "Sandbox feature not available" << std::endl;
            break;
        case core::StatusCode::Internal:
            std::cerr << "Sandbox application failed" << std::endl;
            break;
        default:
            std::cerr << "Unknown error" << std::endl;
    }
}
```

## Platform Support

### Linux

Full support for all sandbox features:
- ✅ Seccomp-BPF filtering
- ✅ AppArmor confinement (if available)
- ✅ Resource limits (rlimits)

### Other Platforms

Graceful degradation:
- ❌ Seccomp (Linux-only)
- ❌ AppArmor (Linux-only)
- ⚠️ Resource limits (POSIX, may have limited support)

## Best Practices

### 1. Principle of Least Privilege

Grant only the minimum syscalls and file access needed:

```cpp
// ❌ Bad: Too permissive
policy.allowed_syscalls = SyscallCategory::Trusted;

// ✅ Good: Minimal required access
policy.allowed_syscalls = SyscallCategory::Lua | SyscallCategory::FileRead;
```

### 2. Resource Limits

Set reasonable limits to prevent resource exhaustion:

```cpp
// ❌ Bad: Unlimited
policy.max_memory_bytes = UINT64_MAX;
policy.max_open_files = UINT32_MAX;

// ✅ Good: Reasonable limits
policy.max_memory_bytes = 64 * 1024 * 1024;   // 64 MiB
policy.max_open_files = 32;
```

### 3. AppArmor Profiles

Use `Complain` mode during development, `Enforce` in production:

```cpp
#ifdef DEBUG
policy.apparmor_level = AppArmorLevel::Complain;
#else
policy.apparmor_level = AppArmorLevel::Enforce;
#endif
```

### 4. Path Allowlisting

Be specific with allowed paths:

```cpp
// ❌ Bad: Too broad
policy.allowed_paths = {"/"};

// ✅ Good: Specific paths
policy.allowed_paths = {
    "/var/pvpgn/plugins/my-plugin",
    "/var/pvpgn/data/read-only"
};
```

## Testing

Run the sandbox unit tests:

```bash
ctest --output-on-failure -R SandboxTest
```

## Troubleshooting

### Seccomp Not Available

**Symptom:** `seccomp not available on this system`

**Solution:**
- Ensure kernel version ≥ 3.17 (seccomp-bpf support)
- Check: `grep CONFIG_SECCOMP_FILTER /boot/config-$(uname -r)`

### AppArmor Not Available

**Symptom:** `AppArmor is not available on this system`

**Solution:**
- Ensure AppArmor is enabled in kernel
- Check: `cat /sys/kernel/security/apparmor/profiles`
- Install libapparmor: `sudo apt-get install libapparmor-dev`

### Permission Denied

**Symptom:** `failed to set PR_SET_NO_NEW_PRIVS`

**Solution:**
- Ensure process has appropriate capabilities
- May require running as root or with CAP_SYS_ADMIN

## References

- [Linux Seccomp Documentation](https://www.kernel.org/doc/html/latest/userspace-api/seccomp_filter.html)
- [AppArmor Documentation](https://gitlab.com/apparmor/apparmor/-/wikis/home)
- [Linux Resource Limits](https://man7.org/linux/man-pages/man2/setrlimit.2.html)
