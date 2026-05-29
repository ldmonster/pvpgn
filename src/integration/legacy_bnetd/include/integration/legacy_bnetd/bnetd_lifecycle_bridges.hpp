// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bnetd_lifecycle_bridges.hpp
/// R245 -- a batch of observation-only strangler-fig bridges for
/// several small bnetd lifecycle / dispatch entry points:
///
///   helpfile    : helpfile_init(filename), helpfile_unload()
///   autoupdate  : autoupdate_load(filename), autoupdate_unload()
///   output      : output_init(), output_write_to_file()
///   support     : support_check_files(supportfile)
///   mail        : handle_mail_command(sd, text), check_mail(sd)
///
/// Each function logs to its own `v3_bnetd_<module>_bridge` module
/// string so downstream filters can target an individual subsystem.
/// All bridges take POD scalars / null-safe C strings only.
/// Contract: every bridge returns 0 (or `void` where the legacy
/// returns void) and legacy MUST fall through.

// helpfile
extern "C" int pvpgn_v3_bnetd_helpfile_init_try(
    const char* filename) noexcept;
extern "C" int pvpgn_v3_bnetd_helpfile_unload_try(void) noexcept;

// autoupdate
extern "C" int pvpgn_v3_bnetd_autoupdate_load_try(
    const char* filename) noexcept;
extern "C" int pvpgn_v3_bnetd_autoupdate_unload_try(void) noexcept;

// output
extern "C" int pvpgn_v3_bnetd_output_init_try(void) noexcept;
extern "C" int pvpgn_v3_bnetd_output_write_to_file_try(void) noexcept;

// support
extern "C" int pvpgn_v3_bnetd_support_check_files_try(
    const char* supportfile) noexcept;

// mail
/// `sd` -- raw socket descriptor of the issuing connection (or -1).
extern "C" int pvpgn_v3_bnetd_mail_handle_command_try(
    int sd,
    const char* text) noexcept;
extern "C" int pvpgn_v3_bnetd_mail_check_try(int sd) noexcept;
