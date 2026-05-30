// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
/// @file wol_internal.hpp
/// Private shared helpers for WolFsm sub-TUs.
///
/// Declares the line-parsing utilities and reply helpers in the
/// pvpgn::protocol::wol namespace so that wol_auth.cpp and wol_chat.cpp
/// can call them without duplicating the implementation.
/// All functions are defined in wol_fsm.cpp (the thin coordinator).

#include <string>
#include <string_view>

namespace pvpgn::protocol::wol {

// ---------------------------------------------------------------------------
// Line-parsing helpers (defined in wol_fsm.cpp anonymous namespace — exposed
// here as free functions for use by sub-TUs)
// ---------------------------------------------------------------------------

/// Split a line into command and params.
/// IRC format: [:<prefix> ] COMMAND [params...] [:<trailing>]
struct ParsedLine {
    std::string command;
    std::string params;  // everything after the command (raw)
};

ParsedLine parse_line(std::string_view line);

/// Extract the trailing parameter (after ':') from a params string.
/// e.g. "target :some text" → trailing = "some text"
std::string_view extract_trailing(std::string_view params);

/// Extract the first token from params (space-delimited).
std::string_view first_token(std::string_view params);

/// Trim leading/trailing whitespace.
std::string_view trim(std::string_view s);

}  // namespace pvpgn::protocol::wol
