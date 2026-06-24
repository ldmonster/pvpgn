// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file help_corpus.hpp
/// In-memory model of the chat `/help` corpus.
///
/// Background: the legacy `src/bnetd/helpfile.cpp` keeps an open
/// `std::FILE*` per language and re-scans the help file on every
/// `/help` invocation. The bridge currently delegates
/// `/help` to that legacy code via `LegacyHelpResponder`.
///
/// This pure in-memory model lets a `FileHelpResponder` consume the
/// corpus directly, eliminating the legacy file I/O and the global
/// `hfd_list` map.
///
/// The model captures only what `/help` actually needs:
///
///   * `HelpEntry::aliases`           -- the command and its aliases,
///                                       each stored with the leading
///                                       `/` (matching router
///                                       conventions and legacy
///                                       `command_get_group` input).
///   * `HelpEntry::description_lines` -- the body lines following
///                                       the `%` header, with full-
///                                       line `#` comments dropped,
///                                       trailing `# ...` truncated,
///                                       and tabs expanded to three
///                                       spaces (matching legacy
///                                       `describe_command`).
///
/// This header is in the `application` layer and therefore must not
/// depend on infrastructure (filesystem, charset, locale). Parsing
/// from an `std::istream` lives next to this model in
/// `help_corpus_parser.hpp`.

#include <string>
#include <string_view>
#include <vector>

namespace pvpgn::application::admin_commands {

struct HelpEntry {
    /// One or more synonyms for the same command. The first entry is
    /// considered the canonical form. All entries include the leading
    /// `/` (e.g. `"/help"`, `"/?"`).
    std::vector<std::string> aliases;

    /// Description body, ready to be sent to the client one line per
    /// `message_send_text` call. Empty / pure-comment lines are
    /// already filtered out by the parser. Tabs are pre-expanded.
    std::vector<std::string> description_lines;
};

class HelpCorpus {
public:
    /// Append a parsed entry. Move-only at the call site; the corpus
    /// is built once by the parser and treated as immutable
    /// thereafter.
    void add(HelpEntry entry) { entries_.push_back(std::move(entry)); }

    [[nodiscard]] const std::vector<HelpEntry>& entries() const noexcept {
        return entries_;
    }

    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] bool        empty() const noexcept { return entries_.empty(); }

    /// Case-insensitive lookup by alias. Accepts both `"/help"` and
    /// `"help"` -- the leading `/` (if present) is stripped before
    /// comparison, matching the legacy `describe_command` contract.
    /// Returns nullptr if no entry matches.
    [[nodiscard]] const HelpEntry* find_by_alias(std::string_view name) const noexcept;

private:
    std::vector<HelpEntry> entries_;
};

}  // namespace pvpgn::application::admin_commands
