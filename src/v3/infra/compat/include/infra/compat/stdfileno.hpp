// SPDX-License-Identifier: GPL-2.0-or-later
//
// Standard file-descriptor number constants.
//
// v3 equivalent of src/compat/stdfileno.h
//
// POSIX defines STDIN_FILENO / STDOUT_FILENO / STDERR_FILENO in <unistd.h>.
// The legacy header papered over platforms that lacked them by defining
// STDINFD / STDOUTFD / STDERRFD macros with hard-coded values 0/1/2.
//
// The v3 version exposes typed `inline constexpr int` constants in the
// `pvpgn::v3::infra::compat` namespace. The names follow the POSIX
// convention (no "FD" suffix) so new code reads naturally.

#pragma once

#if defined(_WIN32) || defined(_WIN64)
// Windows does not provide <unistd.h>; the standard file numbers are
// defined in <io.h> and <stdio.h>.
#  include <io.h>
#  include <stdio.h>
#else
#  include <unistd.h>
#endif

namespace pvpgn::v3::infra::compat {

/// File descriptor for standard input (0).
inline constexpr int kStdinFd  = STDIN_FILENO;

/// File descriptor for standard output (1).
inline constexpr int kStdoutFd = STDOUT_FILENO;

/// File descriptor for standard error (2).
inline constexpr int kStderrFd = STDERR_FILENO;

}  // namespace pvpgn::v3::infra::compat
