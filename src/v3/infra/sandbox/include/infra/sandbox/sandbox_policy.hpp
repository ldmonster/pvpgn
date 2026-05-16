#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace pvpgn::infra::sandbox {

/// Syscall allowlist categories for seccomp filtering
enum class SyscallCategory : uint32_t {
    None        = 0,
    BasicIO     = 1 << 0,   ///< read, write, close, fstat, lseek
    NetworkRecv = 1 << 1,   ///< recv, recvfrom, recvmsg (no send)
    NetworkSend = 1 << 2,   ///< send, sendto, sendmsg
    FileRead    = 1 << 3,   ///< open(O_RDONLY), openat, stat, access
    FileWrite   = 1 << 4,   ///< open(O_WRONLY|O_RDWR), creat, unlink
    Memory      = 1 << 5,   ///< mmap, mprotect, munmap, brk
    Threading   = 1 << 6,   ///< clone, futex, set_robust_list
    Signals     = 1 << 7,   ///< rt_sigaction, rt_sigprocmask
    Time        = 1 << 8,   ///< clock_gettime, gettimeofday, nanosleep
    Process     = 1 << 9,   ///< getpid, getppid, exit, exit_group
    Lua         = BasicIO | Memory | Time | Process,  ///< Minimal set for Lua scripts
    Plugin      = Lua | FileRead | Signals,           ///< Standard plugin set
    Trusted     = 0xFFFFFFFF,                         ///< All syscalls (no restriction)
};

inline SyscallCategory operator|(SyscallCategory a, SyscallCategory b) {
    return static_cast<SyscallCategory>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool operator&(SyscallCategory a, SyscallCategory b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

/// AppArmor profile level
enum class AppArmorLevel {
    Disabled,       ///< No AppArmor confinement
    Complain,       ///< Log violations but don't enforce
    Enforce,        ///< Enforce and deny violations
};

/// Complete sandbox policy for a plugin
struct SandboxPolicy {
    std::string plugin_name;                    ///< For logging/profile naming
    SyscallCategory allowed_syscalls{SyscallCategory::Plugin};
    AppArmorLevel apparmor_level{AppArmorLevel::Disabled};
    std::string apparmor_profile;               ///< Profile name (empty = auto-generate)
    bool enable_seccomp{true};                  ///< Apply seccomp BPF filter
    bool allow_new_privs{false};                ///< PR_SET_NO_NEW_PRIVS
    uint64_t max_memory_bytes{64 * 1024 * 1024}; ///< 64 MiB default
    uint32_t max_open_files{64};                ///< rlimit NOFILE
    std::vector<std::string> allowed_paths;     ///< Paths plugin may read
};

} // namespace pvpgn::infra::sandbox
