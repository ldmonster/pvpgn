// SPDX-License-Identifier: GPL-2.0-or-later

#include "application/admin_commands/file_help_responder.hpp"

#include <string>
#include <string_view>

#include "application/admin_commands/help_command_permissions.hpp"
#include "application/admin_commands/help_corpus.hpp"
#include "application/admin_commands/help_corpus_provider.hpp"
#include "application/admin_commands/message_sink.hpp"

namespace pvpgn::application::admin_commands {

namespace {

constexpr bool is_space(char c) noexcept { return c == ' ' || c == '\t'; }

// Skip the leading command token (e.g. "/help") and the whitespace
// after it; drop a single optional leading `/`; then return the
// remaining whitespace-delimited token. Empty result means "list".
std::string_view help_argument(std::string_view line) noexcept {
    std::size_t i = 0;
    while (i < line.size() && !is_space(line[i])) ++i;       // command word
    while (i < line.size() && is_space(line[i])) ++i;        // whitespace
    if (i < line.size() && line[i] == '/') ++i;              // optional /
    const std::size_t start = i;
    while (i < line.size() && !is_space(line[i])) ++i;
    return line.substr(start, i - start);
}

}  // namespace

bool FileHelpResponder::respond(void* connection,
                                std::string_view command_line) const
{
    const HelpCorpus* corpus = corpora_.for_connection(connection);
    if (corpus == nullptr) {
        sink_.send(
            connection, IMessageSink::Severity::Error,
            "Oops ! There is a problem with the help file. "
            "Please contact the administrator of the server.");
        return false;
    }

    const std::string_view arg = help_argument(command_line);

    if (arg.empty()) {
        // List mode.
        sink_.send(connection, IMessageSink::Severity::Info,
                   "Chat commands : ");
        for (const auto& entry : corpus->entries()) {
            if (entry.aliases.empty()) continue;
            if (!permissions_.is_visible(connection, entry.aliases.front())) {
                continue;
            }
            std::string buffer;
            for (const auto& alias : entry.aliases) {
                buffer.push_back(' ');
                buffer.append(alias);
            }
            sink_.send(connection, IMessageSink::Severity::Info, buffer);
        }
        return true;
    }

    // Describe mode.
    const HelpEntry* entry = corpus->find_by_alias(arg);
    if (entry == nullptr) {
        sink_.send(connection, IMessageSink::Severity::Error,
                   "No help available for that command");
        return false;
    }
    for (const auto& line : entry->description_lines) {
        const auto severity =
            (!line.empty() && line.front() == '/')
                ? IMessageSink::Severity::Error
                : IMessageSink::Severity::Info;
        sink_.send(connection, severity, line);
    }
    return true;
}

}  // namespace pvpgn::application::admin_commands
