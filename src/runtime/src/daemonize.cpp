// SPDX-License-Identifier: GPL-2.0-or-later
/// Daemonization utilities for Unix systems
/// Handles fork, setsid, umask, and working directory setup

#include "runtime/service_host.hpp"

#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <csignal>
#include <syslog.h>
#include <cstring>
#include <cerrno>
#endif

namespace pvpgn::runtime {

#ifndef _WIN32

/// Daemonize the process on Unix systems
/// Performs standard daemonization: fork, setsid, chdir, umask, close fds
class Daemonizer {
public:
    /// Daemonize the current process
    /// Returns error message if daemonization fails
    static Result<void, std::string> daemonize(const ServiceConfig& config)
    {
        // Don't daemonize if running in foreground mode
        if (config.foreground) {
            return Result<void, std::string>();  // Success
        }
        
        // First fork: detach from controlling terminal
        pid_t pid = fork();
        if (pid < 0) {
            return Result<void, std::string>(
                core::fail(std::string("First fork failed: ") + std::strerror(errno))
            );
        }
        if (pid > 0) {
            // Parent process exits
            std::exit(0);
        }
        
        // Become session leader
        if (setsid() < 0) {
            return Result<void, std::string>(
                core::fail(std::string("setsid failed: ") + std::strerror(errno))
            );
        }
        
        // Ignore SIGHUP to prevent termination if terminal closes
        std::signal(SIGHUP, SIG_IGN);
        
        // Second fork: prevent re-acquiring controlling terminal
        pid = fork();
        if (pid < 0) {
            return Result<void, std::string>(
                core::fail(std::string("Second fork failed: ") + std::strerror(errno))
            );
        }
        if (pid > 0) {
            // Parent process exits
            std::exit(0);
        }
        
        // Change working directory
        if (!config.work_dir.empty()) {
            if (chdir(config.work_dir.c_str()) < 0) {
                return Result<void, std::string>(
                    core::fail(std::string("chdir to ") + config.work_dir + " failed: " + std::strerror(errno))
                );
            }
        }
        
        // Set umask to restrict file permissions
        umask(0027);  // rw-r----- for files, rwx------ for directories
        
        // Close standard file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
        // Redirect to /dev/null
        int fd = open("/dev/null", O_RDWR);
        if (fd < 0) {
            return Result<void, std::string>(
                core::fail(std::string("Failed to open /dev/null: ") + std::strerror(errno))
            );
        }
        
        // Duplicate to stdin, stdout, stderr
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        
        if (fd > 2) {
            close(fd);
        }
        
        // Change user/group if specified
        if (!config.run_as_user.empty() || !config.run_as_group.empty()) {
            auto result = change_user_group(config);
            if (!result.has_value()) {
                return result;
            }
        }
        
        // Write PID file if specified
        if (!config.pid_file.empty()) {
            auto result = write_pid_file(config.pid_file);
            if (!result.has_value()) {
                return result;
            }
        }
        
        return Result<void, std::string>();  // Success
    }
    
private:
    static Result<void, std::string> change_user_group(const ServiceConfig& config)
    {
        gid_t gid = static_cast<gid_t>(-1);
        uid_t uid = static_cast<uid_t>(-1);
        
        // Get group ID if specified
        if (!config.run_as_group.empty()) {
            struct group* grp = getgrnam(config.run_as_group.c_str());
            if (!grp) {
                return Result<void, std::string>(
                    core::fail(std::string("Group not found: ") + config.run_as_group)
                );
            }
            gid = grp->gr_gid;
        }
        
        // Get user ID if specified
        if (!config.run_as_user.empty()) {
            struct passwd* pwd = getpwnam(config.run_as_user.c_str());
            if (!pwd) {
                return Result<void, std::string>(
                    core::fail(std::string("User not found: ") + config.run_as_user)
                );
            }
            uid = pwd->pw_uid;
            
            // Use user's primary group if group not explicitly specified
            if (gid == static_cast<gid_t>(-1)) {
                gid = pwd->pw_gid;
            }
        }
        
        // Change group first (requires root)
        if (gid != static_cast<gid_t>(-1)) {
            if (setgid(gid) < 0) {
                return Result<void, std::string>(
                    core::fail(std::string("setgid failed: ") + std::strerror(errno))
                );
            }
        }
        
        // Then change user (requires root)
        if (uid != static_cast<uid_t>(-1)) {
            if (setuid(uid) < 0) {
                return Result<void, std::string>(
                    core::fail(std::string("setuid failed: ") + std::strerror(errno))
                );
            }
        }
        
        return Result<void, std::string>();  // Success
    }
    
    static Result<void, std::string> write_pid_file(const std::string& pid_file)
    {
        int fd = open(pid_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            return Result<void, std::string>(
                core::fail(std::string("Failed to open PID file: ") + std::strerror(errno))
            );
        }
        
        pid_t pid = getpid();
        std::string pid_str = std::to_string(pid) + "\n";
        
        if (write(fd, pid_str.c_str(), pid_str.length()) < 0) {
            close(fd);
            return Result<void, std::string>(
                core::fail(std::string("Failed to write PID file: ") + std::strerror(errno))
            );
        }
        
        close(fd);
        return Result<void, std::string>();  // Success
    }
};

#else

/// Windows stub for daemonization (no-op on Windows)
class Daemonizer {
public:
    static Result<void, std::string> daemonize(const ServiceConfig& config)
    {
        // Windows doesn't have daemonization in the Unix sense
        // Services are managed by the Service Control Manager
        return Result<void, std::string>();  // Success
    }
};

#endif

} // namespace pvpgn::runtime
