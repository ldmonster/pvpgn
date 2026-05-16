#include "infra/sandbox/seccomp_filter.hpp"
#include "core/error.hpp"
#include <vector>
#include <cstring>
#include <cerrno>

#ifdef __linux__
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#endif

namespace pvpgn::infra::sandbox {

bool SeccompFilter::is_available() noexcept {
#ifdef __linux__
    // Try to check if seccomp is available via prctl
    int result = prctl(PR_GET_SECCOMP, 0, 0, 0, 0);
    // If we get EINVAL, seccomp is not available
    // Any other result (including 0 or 1) means it's available
    return result != -1 || errno != EINVAL;
#else
    return false;
#endif
}

std::vector<int> SeccompFilter::syscalls_for_category(SyscallCategory category) {
    std::vector<int> syscalls;

    if (category == SyscallCategory::None) {
        return syscalls;
    }

#ifdef __linux__
    // BasicIO: read, write, close, fstat, lseek
    if (category & SyscallCategory::BasicIO) {
        syscalls.push_back(__NR_read);
        syscalls.push_back(__NR_write);
        syscalls.push_back(__NR_close);
        syscalls.push_back(__NR_fstat);
        syscalls.push_back(__NR_lseek);
    }

    // Memory: mmap, mprotect, munmap, brk
    if (category & SyscallCategory::Memory) {
        syscalls.push_back(__NR_mmap);
        syscalls.push_back(__NR_mprotect);
        syscalls.push_back(__NR_munmap);
        syscalls.push_back(__NR_brk);
    }

    // Time: clock_gettime, gettimeofday, nanosleep
    if (category & SyscallCategory::Time) {
        syscalls.push_back(__NR_clock_gettime);
        syscalls.push_back(__NR_gettimeofday);
        syscalls.push_back(__NR_nanosleep);
    }

    // Process: getpid, exit, exit_group
    if (category & SyscallCategory::Process) {
        syscalls.push_back(__NR_getpid);
        syscalls.push_back(__NR_exit);
        syscalls.push_back(__NR_exit_group);
    }

    // FileRead: open, openat, stat, fstat, access
    if (category & SyscallCategory::FileRead) {
        syscalls.push_back(__NR_open);
        syscalls.push_back(__NR_openat);
        syscalls.push_back(__NR_stat);
        syscalls.push_back(__NR_fstat);
        syscalls.push_back(__NR_access);
    }

    // FileWrite: creat, unlink, rename
    if (category & SyscallCategory::FileWrite) {
        syscalls.push_back(__NR_creat);
        syscalls.push_back(__NR_unlink);
        syscalls.push_back(__NR_rename);
    }

    // NetworkRecv: recv, recvfrom, recvmsg
    if (category & SyscallCategory::NetworkRecv) {
#ifdef __NR_recv
        syscalls.push_back(__NR_recv);
#endif
#ifdef __NR_recvfrom
        syscalls.push_back(__NR_recvfrom);
#endif
#ifdef __NR_recvmsg
        syscalls.push_back(__NR_recvmsg);
#endif
    }

    // NetworkSend: send, sendto, sendmsg
    if (category & SyscallCategory::NetworkSend) {
#ifdef __NR_send
        syscalls.push_back(__NR_send);
#endif
#ifdef __NR_sendto
        syscalls.push_back(__NR_sendto);
#endif
#ifdef __NR_sendmsg
        syscalls.push_back(__NR_sendmsg);
#endif
    }

    // Threading: clone, futex, set_robust_list
    if (category & SyscallCategory::Threading) {
        syscalls.push_back(__NR_clone);
        syscalls.push_back(__NR_futex);
#ifdef __NR_set_robust_list
        syscalls.push_back(__NR_set_robust_list);
#endif
    }

    // Signals: rt_sigaction, rt_sigprocmask
    if (category & SyscallCategory::Signals) {
        syscalls.push_back(__NR_rt_sigaction);
        syscalls.push_back(__NR_rt_sigprocmask);
    }
#endif

    return syscalls;
}

core::Result<void, core::Error> SeccompFilter::apply(const SandboxPolicy& policy) {
#ifndef __linux__
    return core::fail(core::make_error(core::StatusCode::Unavailable, "seccomp not available on this platform"));
#else
    if (!is_available()) {
        return core::fail(core::make_error(core::StatusCode::Unavailable, "seccomp not available on this system"));
    }

    // Get the list of allowed syscalls
    auto allowed = syscalls_for_category(policy.allowed_syscalls);

    // Build the BPF program
    // Structure: load syscall number, check each allowed syscall, default deny
    std::vector<struct sock_filter> filter;

    // Load the syscall number from seccomp_data.nr (offset 0)
    struct sock_filter load_stmt = BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr));
    filter.push_back(load_stmt);

    // For each allowed syscall, add a jump that allows it
    for (size_t i = 0; i < allowed.size(); ++i) {
        int syscall_nr = allowed[i];
        // Jump to allow if equal, otherwise continue to next check
        // The jump offset is: (allowed.size() - i) to skip to the final ALLOW
        struct sock_filter jump_stmt = BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, syscall_nr, 0, 1);
        filter.push_back(jump_stmt);
        struct sock_filter allow_stmt = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW);
        filter.push_back(allow_stmt);
    }

    // Default: kill the process
    struct sock_filter kill_stmt = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS);
    filter.push_back(kill_stmt);

    // Create the BPF program
    struct sock_fprog prog;
    prog.len = static_cast<unsigned short>(filter.size());
    prog.filter = filter.data();

    // Set NO_NEW_PRIVS if requested
    if (!policy.allow_new_privs) {
        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
            return core::fail(core::make_error(core::StatusCode::Internal, "failed to set PR_SET_NO_NEW_PRIVS"));
        }
    }

    // Apply the seccomp filter
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) < 0) {
        return core::fail(core::make_error(core::StatusCode::Internal, "failed to apply seccomp filter"));
    }

    return core::Result<void, core::Error>();
#endif
}

} // namespace pvpgn::infra::sandbox
