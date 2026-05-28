// SPDX-License-Identifier: GPL-2.0-or-later

#include "integration/legacy_bnetd/legacy_help_command_permissions.hpp"

#include <string>
#include <string_view>

#include "common/setup_before.h"
#include "command_groups.h"
#include "connection.h"
#include "account.h"
#include "account_wrap.h"
#include "common/setup_after.h"

namespace pvpgn::integration::legacy_bnetd {

bool LegacyHelpCommandPermissions::is_visible(void* connection,
                                              std::string_view canonical_name) const
{
    if (connection == nullptr || canonical_name.empty()) return false;
    auto* c = static_cast<::pvpgn::bnetd::t_connection*>(connection);
    auto* a = ::pvpgn::bnetd::conn_get_account(c);
    if (a == nullptr) return false;
    const std::string name{canonical_name};  // need NUL-terminated
    const unsigned group_mask = ::pvpgn::bnetd::command_get_group(name.c_str());
    const unsigned account_mask = ::pvpgn::bnetd::account_get_command_groups(a);
    return (group_mask & account_mask) != 0u;
}

}  // namespace pvpgn::integration::legacy_bnetd
