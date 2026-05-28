// SPDX-License-Identifier: GPL-2.0-or-later
//
// R216d: legacy adapter for `application::admin_commands::IHelpResponder`.
// Delegates straight into the legacy `handle_help_command` symbol.
// Lives in `integration_legacy_bnetd_linked` because it needs to
// include the legacy `helpfile.h` header.

#include "integration/legacy_bnetd/legacy_help_responder.hpp"

#include <string>
#include <string_view>

#include "common/setup_before.h"
#include "connection.h"
#include "helpfile.h"
#include "common/setup_after.h"

namespace pvpgn::integration::legacy_bnetd {

bool LegacyHelpResponder::respond(void* connection,
                                  std::string_view command_line) const
{
    if (connection == nullptr) return false;
    auto* c = static_cast<::pvpgn::bnetd::t_connection*>(connection);
    // The legacy API takes a NUL-terminated `char const*`. The bridge
    // hands us the exact pointer it received from the legacy
    // dispatch, so we copy here to keep the contract obvious in case
    // a future call site passes a non-NUL-terminated view.
    const std::string text{command_line};
    const int rc = ::pvpgn::bnetd::handle_help_command(c, text.c_str());
    return rc == 0;
}

}  // namespace pvpgn::integration::legacy_bnetd
