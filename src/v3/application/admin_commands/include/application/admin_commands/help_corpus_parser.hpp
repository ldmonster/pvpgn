// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file help_corpus_parser.hpp
/// Parse a help corpus stream (`.lst`-style) into a `HelpCorpus`.
///
/// Grammar (matches the legacy `helpfile.cpp` reader, line-for-line):
///
/// * Lines are processed in order.
/// * A line whose first non-whitespace character is `%` starts a new
///   command entry. The rest of the line, up to an optional trailing
///   `# comment`, is split on whitespace into the command and its
///   aliases. The leading `%` is stripped from the first token.
///   Each name is then prefixed with `/`.
/// * Any subsequent line that is NOT another `%` line is part of the
///   current entry's description, with these transformations:
///     - Full-line `#` comments (first non-blank is `#`) are dropped.
///     - Trailing `# ...` on a description line is truncated at the
///       first `#`.
///     - Tabs are expanded to three spaces.
///     - Resulting empty lines are dropped.
/// * Lines appearing before the first `%` line are ignored (legacy
///   behaviour: they would have been silently skipped).
/// * Parsing does not fail on malformed input; the worst case is an
///   empty corpus. The `Result` form is reserved for future I/O
///   errors propagated by the caller.

#include <iosfwd>

#include "core/result.hpp"

#include "application/admin_commands/help_corpus.hpp"

namespace pvpgn::application::admin_commands {

[[nodiscard]] core::Result<HelpCorpus, core::Error>
parse_help_corpus(std::istream& in);

}  // namespace pvpgn::application::admin_commands
