# Plugin Sandboxing Integration Guide

> **Updated for PvPGN v3 R347** — This document covers both the legacy
> `infra/sandbox/` infrastructure and the new lightweight seccomp wrapper
> introduced in R347 (`src/v3/infra/plugin/`).

---

## Overview

PvPGN v3 provides two complementary sandboxing layers for plugin isolation:

| Layer | Location | Scope |
|-------|----------|-------|
| **Legacy full sandbox** | `src/v3/infra/sandbox/` | AppArmor + rlimits + seccomp categories |
| **R347 lightweight sandbox** | `src/v3/infra/plugin/` | Minimal seccomp allowlist for native plugin callbacks |

Both layers are opt-in and can be combined for defence-in-depth.

---

## R347 Lightweight Sandbox (`pvpgn_infra_plugin`)

### Enabling seccomp

Pass `-DPVPGN_V3_WITH_SECCOMP=ON` to CMake:

```bash
cmake -B build -DPVPGN_V3_WITH_SECCOMP=ON ..
cmake --build build
```

This requires `libseccomp` development headers:

```bash
# Debian/Ubuntu
sudo apt-get install libseccomp-dev

# Fedora/RHEL
sudo dnf install libseccomp-devel

# Alpine
apk add libseccomp-dev
```

### Using `run_sandboxed()`

```cpp
#include "infra/plugin/sandbox.hpp"

using namespace pvpgn::infra::plugin;

// Wrap a plugin init call in the sandbox:
int rc = run_sandboxed([&]() -> int {
    return plugin_init_fn(&ctx);
});
```

On Linux with seccomp enabled, the lambda runs with the syscall allowlist
active.  On other platforms it runs without restriction.

### Checking availability at runtime

```cpp
if (sandbox_available()) {
    // seccomp is compiled in and active
} else {
    // running without OS-level sandboxing
}
```

---

## Syscall Allowlist Rationale

The R347 allowlist is intentionally minimal — it covers only what a well-behaved
plugin callback needs:

| Syscall | Rationale |
|---------|-----------|
| `read` | Read from already-open file descriptors (e.g. config files opened before sandbox) |
| `write` | Write to already-open file descriptors (e.g. log pipe) |
| `mmap` | Heap allocation (`malloc`/`new`) |
| `munmap` | Heap deallocation (`free`/`delete`) |
| `brk` | Heap growth (glibc allocator) |
| `exit` | Normal thread exit |
| `exit_group` | Normal process exit |
| `futex` | Mutex / condition variable (C++ standard library) |
| `clock_gettime` | Monotonic / wall-clock time queries |
| `gettimeofday` | Legacy time queries |

Default action: **`SCMP_ACT_KILL`** — any syscall not in the list terminates
the process immediately.

### Extending the allowlist

If your plugin legitimately needs additional syscalls, add them in
[`src/v3/infra/plugin/src/sandbox.cpp`](../src/v3/infra/plugin/src/sandbox.cpp)
inside the `#ifdef PVPGN_V3_SECCOMP_ENABLED` block:

```cpp
// Example: allow getpid for plugins that need their own PID
if (!allow(SCMP_SYS(getpid))) return false;
```

Document the rationale in a comment next to the rule.

### Strict mode

By default, if seccomp setup fails (e.g. the kernel does not support it),
`run_sandboxed()` falls back to running the callback without sandboxing and
logs a warning.

To make sandbox failures fatal, set the environment variable:

```bash
export PVPGN_PLUGIN_STRICT_SANDBOX=1
```

Or define the compile-time macro:

```cmake
target_compile_definitions(my_target PRIVATE PVPGN_PLUGIN_STRICT_SANDBOX)
```

In strict mode, `run_sandboxed()` throws `std::runtime_error` if seccomp
setup fails.

---

## Non-Linux Fallback Behaviour

On macOS, Windows, and other non-Linux platforms:

- `sandbox_available()` returns `false`.
- `run_sandboxed(fn)` calls `fn()` directly without any OS-level restriction.
- No compile-time errors or warnings are emitted.
- The `PVPGN_V3_WITH_SECCOMP` option is accepted but has no effect.

This allows the same plugin code to compile and run on all platforms while
providing hardening on Linux production deployments.

---

## Legacy Full Sandbox (`infra/sandbox/`)

The legacy sandbox provides a richer policy model with AppArmor confinement,
rlimits, and categorised syscall allowlists.

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
    auto error = std::move(result).error();
    std::cerr << "Sandbox failed: " << error.message() << std::endl;
    return;
}

// Now load and run the plugin
// ...
```

### Syscall Categories

The `SyscallCategory` enum defines allowlists for different types of plugins:

| Category | Syscalls | Use Case |
|----------|----------|----------|
| `Basic` | read, write, mmap, munmap, brk, exit, exit_group, futex, clock_gettime, gettimeofday | Minimal computation |
| `Network` | Basic + socket, connect, send, recv, poll | Network-enabled plugins |
| `FileIO` | Basic + open, openat, close, stat, fstat, lstat, read, write, lseek | File-access plugins |
| `Full` | All of the above | Trusted plugins |

---

## Choosing the Right Sandbox Layer

| Scenario | Recommendation |
|----------|---------------|
| New native plugin, Linux production | Use R347 `run_sandboxed()` + legacy `SandboxPolicy` |
| Lua plugin | Legacy sandbox only (Lua VM provides its own isolation) |
| Development / testing | Disable seccomp (`PVPGN_V3_WITH_SECCOMP=OFF`) |
| macOS / Windows | No OS sandbox; rely on Lua VM isolation |
| Untrusted third-party plugin | Both layers + AppArmor profile |

---

## See Also

- [`src/v3/infra/plugin/include/infra/plugin/sandbox.hpp`](../src/v3/infra/plugin/include/infra/plugin/sandbox.hpp) — R347 API
- [`src/v3/infra/plugin/include/infra/plugin/api.h`](../src/v3/infra/plugin/include/infra/plugin/api.h) — C ABI 1.0
- [`docs/plugin-versioning-guide.md`](plugin-versioning-guide.md) — Plugin versioning
- [`docs/lua-api-v2.md`](lua-api-v2.md) — Lua API v2 reference
