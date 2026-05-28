// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file legacy_help_responder.hpp
/// Concrete `IHelpResponder` that delegates to the legacy
/// `handle_help_command` in `src/bnetd/helpfile.cpp`. Lives in
/// `integration_legacy_bnetd_linked` because it needs the legacy
/// headers (it casts the opaque connection handle to
/// `pvpgn::bnetd::t_connection*`).
///
/// R216d strangler step: the v3 bridge no longer references
/// `handle_help_command` directly; it talks to this adapter. A future
/// round will introduce a `FileHelpResponder` that parses the help
/// corpus directly without legacy dependencies, at which point this
/// adapter can be retired.

#include <string_view>

#include "application/admin_commands/help_responder.hpp"

namespace pvpgn::integration::legacy_bnetd {

class LegacyHelpResponder final
    : public application::admin_commands::IHelpResponder {
public:
    bool respond(void* connection,
                 std::string_view command_line) const override;
};

}  // namespace pvpgn::integration::legacy_bnetd
