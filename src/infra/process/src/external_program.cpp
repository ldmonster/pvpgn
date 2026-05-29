// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/process/external_program.hpp"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(_WIN32)
#  define PVPGN_V3_PROCESS_HAS_FORK 0
#else
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <unistd.h>
#  define PVPGN_V3_PROCESS_HAS_FORK 1
#endif

namespace pvpgn::infra::process {

core::Result<CaptureResult, core::Error>
run_capture(std::string_view command) {
    if (command.empty()) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "empty command"));
    }

#if !PVPGN_V3_PROCESS_HAS_FORK
    (void)command;
    return core::fail(core::make_error(core::StatusCode::Unimplemented,
                                       "subprocess unsupported on this host"));
#else
    std::string cmd_owned(command);

    int fds[2];
    if (::pipe(fds) < 0) {
        return core::fail(core::make_error(core::StatusCode::Internal,
                                           "pipe failed"));
    }

    ::pid_t child = ::fork();
    if (child < 0) {
        ::close(fds[0]);
        ::close(fds[1]);
        return core::fail(core::make_error(core::StatusCode::Internal,
                                           "fork failed"));
    }

    if (child == 0) {
        // Child: redirect stdout+stderr to pipe write-end.
        ::close(fds[0]);

        ::close(STDIN_FILENO);
        if (fds[1] != STDOUT_FILENO) ::dup2(fds[1], STDOUT_FILENO);
        if (fds[1] != STDERR_FILENO) ::dup2(fds[1], STDERR_FILENO);
        if (fds[1] != STDOUT_FILENO && fds[1] != STDERR_FILENO) {
            ::close(fds[1]);
        }

        ::execlp(cmd_owned.c_str(), cmd_owned.c_str(),
                 static_cast<char*>(nullptr));
        // exec failed
        std::_Exit(127);
    }

    // Parent
    ::close(fds[1]);

    std::string out;
    {
        std::array<char, 4096> buf{};
        ssize_t                n;
        while ((n = ::read(fds[0], buf.data(), buf.size())) > 0) {
            out.append(buf.data(), static_cast<std::size_t>(n));
        }
    }
    ::close(fds[0]);

    int status = 0;
    while (::waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            status = -1;
            break;
        }
    }

    return CaptureResult{status, std::move(out)};
#endif
}

}  // namespace pvpgn::infra::process
