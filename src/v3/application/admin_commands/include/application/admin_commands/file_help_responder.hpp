// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file file_help_responder.hpp
/// Pure-v3 `IHelpResponder` implementation backed by an
/// `IHelpCorpusProvider`, an `IHelpCommandPermissions` policy, and an
/// `IMessageSink`. Reproduces the behaviour of legacy
/// `helpfile.cpp::handle_help_command`.
///
/// Behaviour summary (matches legacy line-for-line):
///
/// * Parse the command line: drop the command word (e.g. `/help`),
///   skip whitespace, drop a single leading `/`, then read the next
///   whitespace-delimited token. The token is the argument; empty
///   token means "list mode".
/// * **List mode**: send a leading "Chat commands :" header
///   (informational), then for each `HelpEntry` whose canonical
///   alias passes the permission check, send a single line of the
///   form ` /cmd /alias /alias2` (space-separated, leading space).
/// * **Describe mode**: look up the argument via
///   `HelpCorpus::find_by_alias`. On hit, send every description
///   line. Lines starting with `/` are sent as `Severity::Error`,
///   matching the legacy "make-it-coloured" highlight; everything
///   else as `Severity::Info`. On miss, send "No help available for
///   that command" as `Severity::Error`.
/// * If `IHelpCorpusProvider::for_connection` returns nullptr,
///   send the legacy "There is a problem with the help file"
///   message (Error) and return false so the bridge can fall back.
///
/// Permission filter: legacy `list_commands` evaluates the filter
/// once per alias and uses only the LAST alias's result; this v3
/// implementation evaluates it once per entry against the first
/// (canonical) alias. Practically equivalent for all in-tree help
/// entries and noted in `plans/r216f-checklist.md`.

#include "application/admin_commands/help_responder.hpp"

namespace pvpgn::application::admin_commands {

class IHelpCorpusProvider;
class IHelpCommandPermissions;
class IMessageSink;

class FileHelpResponder final : public IHelpResponder {
public:
    FileHelpResponder(const IHelpCorpusProvider& corpora,
                      const IHelpCommandPermissions& permissions,
                      const IMessageSink& sink) noexcept
        : corpora_(corpora), permissions_(permissions), sink_(sink) {}

    bool respond(void* connection,
                 std::string_view command_line) const override;

private:
    const IHelpCorpusProvider&     corpora_;
    const IHelpCommandPermissions& permissions_;
    const IMessageSink&            sink_;
};

}  // namespace pvpgn::application::admin_commands
