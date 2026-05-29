// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/legacy_whisper_target_lookup.hpp"

#include <string>

#include "common/setup_before.h"
#include "common/setup_after.h"
#include "bnetd/connection.h"

namespace pvpgn::integration::legacy_bnetd {

application::chat::WhisperTarget LegacyWhisperTargetLookup::lookup(
    std::string_view sender, std::string_view target) const noexcept {
    application::chat::WhisperTarget out{};
    if (target.empty()) return out;
    // Legacy lookup helpers want a NUL-terminated string.
    std::string name(target);
    auto* dest_c = ::pvpgn::bnetd::connlist_find_connection_by_accountname(
        name.c_str());
    if (dest_c == nullptr) {
        // Offline (or pre-login).
        out.online = false;
        return out;
    }
    out.online = true;
    out.dnd = (::pvpgn::bnetd::conn_get_dndstr(dest_c) != nullptr);
    // 23a: ignored-by-target. Ask the target's connection whether
    // its ignore list contains `sender`. `conn_check_ignoring` returns
    //   1 -> ignored
    //   0 -> not ignored
    //  -1 -> error (NULL conn or unknown sender account); treat as
    //        "not ignored" so we don't silently swallow whispers on
    //        lookup failures.
    if (!sender.empty()) {
        std::string sender_z(sender);
        out.ignored_by =
            (::pvpgn::bnetd::conn_check_ignoring(dest_c, sender_z.c_str())
                == 1);
    } else {
        out.ignored_by = false;
    }
    return out;
}

}  // namespace pvpgn::integration::legacy_bnetd
