// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file help_responder.hpp
/// Port for replying to a chat `/help` (or `/?`) command.
///
/// This port exists so the R216 strangler-fig bridge does not have to
/// hard-code a call to legacy `handle_help_command` in
/// `src/bnetd/helpfile.cpp`. The bridge holds an `IHelpResponder&`,
/// and the wiring layer (`integration_legacy_bnetd_linked`) supplies
/// a concrete implementation.
///
/// Two implementations are expected:
///   * `LegacyHelpResponder` (R216d) -- delegates straight to
///     legacy `handle_help_command(t_connection*, char const*)`.
///     Lives in `integration_legacy_bnetd_linked` because it needs
///     the legacy headers.
///   * `FileHelpResponder` (future) -- pure v3, parses the help
///     corpus (a `.lst` file) directly without legacy dependencies.
///
/// The `connection` parameter is opaque (`void*`) on purpose: the
/// port lives in `application/` and must not name legacy types.
/// Implementations cast back as needed.

#include <string_view>

namespace pvpgn::application::admin_commands {

class IHelpResponder {
public:
    virtual ~IHelpResponder() = default;

    /// Handle a `/help` or `/?` command line.
    ///
    /// @param connection  opaque connection handle. The legacy
    ///                    implementation casts it to
    ///                    `pvpgn::bnetd::t_connection*`.
    /// @param command_line full command text as received from the
    ///                    client (e.g. `"/help"`, `"/? channels"`).
    ///
    /// @return true if the responder consumed the command and sent
    ///         text back to the client; false if the responder
    ///         could not produce a response (the bridge will fall
    ///         back to legacy dispatch in that case).
    virtual bool respond(void* connection,
                         std::string_view command_line) const = 0;
};

}  // namespace pvpgn::application::admin_commands
