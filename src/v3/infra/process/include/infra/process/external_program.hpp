// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file external_program.hpp
/// Cross-platform external-program launcher with stdout capture.
///
/// Replaces the legacy `runprog_open / runprog_close` pair from
/// `src/bnetd/runprog.cpp`. The legacy variant returned a `FILE*` over
/// a fork/exec'd subprocess's stdout. The v3 version offers a
/// `run_capture(argv0)` helper that returns the captured stdout as a
/// string and the child's wait-status.
///
/// On POSIX hosts the implementation uses `pipe + fork + execlp`. On
/// Windows the implementation returns `Unsupported` -- matches the
/// legacy `#ifndef DO_SUBPROC` always-fail branch.

#include <cstdint>
#include <string>
#include <string_view>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::infra::process {

/// Result of a finished child process.
struct CaptureResult {
    int         exit_status;  ///< raw waitpid status (POSIX semantics)
    std::string stdout_text;  ///< concatenated stdout, decoded as bytes
};

/// Spawn `command` via `execlp(command, command, NULL)` and capture
/// its stdout+stderr until EOF. Returns the wait-status and captured
/// bytes. The child inherits no fds beyond the pipe write-end.
///
/// Errors:
///   * `Unimplemented` -- platform has no subprocess support.
///   * `InvalidArgument` -- empty `command`.
///   * `Internal` -- pipe / fork / fdopen failure.
core::Result<CaptureResult, core::Error>
run_capture(std::string_view command);

}  // namespace pvpgn::infra::process
