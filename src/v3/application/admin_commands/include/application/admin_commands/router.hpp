// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file router.hpp
/// Pure-v3 routing decision for legacy bnetd chat commands that have
/// been migrated to the v3 strangler-fig pipeline.
///
/// The legacy `handle_command()` in `src/bnetd/command.cpp` calls
/// `pvpgn_v3_command_dispatch_try()` for each incoming command. The
/// bridge in `integration_legacy_bnetd_linked` adapts the legacy
/// `t_connection*` world to this router. This header owns ONLY the
/// pure decision logic: name normalisation, alias resolution, and the
/// permission predicate hand-off. The bridge owns I/O (sending text
/// back to the client, userlog append).
///
/// Keeping this logic pure-v3 makes it unit-testable without any
/// legacy types and decouples the dispatch decision from the legacy
/// flood/lua/userlog machinery.

#include <functional>
#include <string>
#include <string_view>

namespace pvpgn::application::admin_commands {

/// Outcome of routing a single chat command line.
enum class RouteAction {
    /// The command name is not one this router recognises -- the
    /// bridge should return 0 so legacy dispatch handles it.
    NotFound,

    /// The command IS recognised, but the caller lacks the required
    /// permission group. The bridge should send the legacy
    /// "reserved for admins" message and return 1 (consumed).
    Denied,

    /// The command IS recognised AND the caller is permitted. The
    /// bridge should invoke the matching handler, send its output
    /// via legacy `message_send_text`, append to userlog, and
    /// return 1 (consumed).
    Handled,
};

/// Canonical command name plus action.
struct RouteDecision {
    RouteAction action;

    /// Canonical command name (always begins with `/`). When
    /// `action == NotFound` this is empty.
    std::string canonical_name;
};

/// Permission predicate signature -- the bridge supplies one that
/// adapts to legacy `command_get_group(name) &
/// account_get_command_groups(account)`.
///
/// Should return true iff the caller may execute the named command.
using PermissionPredicate =
    std::function<bool(std::string_view canonical_name)>;

/// Route a legacy chat command line.
///
/// @param command_line  Full text as received from the client (e.g.
///                      "/version", "/uptime ", "/ver foo bar").
///                      Only the first whitespace-delimited token is
///                      examined.
/// @param is_permitted  Predicate invoked once with the canonical
///                      command name when a match is found.
///
/// Recognised aliases (R216 first migration set + R216b extension):
///   /version, /ver   -> canonical "/version"
///   /uptime          -> canonical "/uptime"
///   /help, /?        -> canonical "/help"
///   /who             -> canonical "/who"      (R216b)
///   /whoami          -> canonical "/whoami"   (R216b)
///   /users           -> canonical "/users"    (R216b, alias of legacy /status)
///   /finger          -> canonical "/finger"   (R216b)
[[nodiscard]] RouteDecision
route(std::string_view command_line, const PermissionPredicate& is_permitted);

}  // namespace pvpgn::application::admin_commands
