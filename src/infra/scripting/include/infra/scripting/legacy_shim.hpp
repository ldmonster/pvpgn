// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/**
 * @file legacy_shim.hpp
 * @brief Legacy bnetd_* → pvpgn.* Lua compatibility shim (R349).
 *
 * Call `install_legacy_shim(lua)` after `register_v2_api()` to install
 * backward-compatible `bnetd_*` globals that delegate to `pvpgn.*`.
 *
 * ### Migration table
 * | Legacy (bnetd_*)              | New (pvpgn.*)                    |
 * |-------------------------------|----------------------------------|
 * | bnetd_send_message(u, msg)    | pvpgn.send_chat(u, msg)          |
 * | bnetd_get_account_info(u)     | pvpgn.get_account(u)             |
 * | bnetd_ban_user(u, reason)     | pvpgn.ban_account(u, reason)     |
 * | bnetd_kick_user(u, reason)    | pvpgn.kick_user(u, reason)       |
 * | bnetd_broadcast(ch, msg)      | pvpgn.broadcast(ch, msg)         |
 * | bnetd_log(level, msg)         | pvpgn.log(level, msg)            |
 */

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace pvpgn::infra::scripting {

/**
 * @brief Install the legacy bnetd_* → pvpgn.* compatibility shim.
 *
 * Executes a Lua snippet that creates `bnetd_*` global functions delegating
 * to the corresponding `pvpgn.*` functions.  Must be called after
 * `register_v2_api()` has populated the `pvpgn` table.
 *
 * If the `pvpgn` table is not present (e.g. Lua API v2 not loaded), the
 * shim is silently skipped.
 *
 * @param lua  The Lua state to install the shim into.
 */
void install_legacy_shim(sol::state& lua);

} // namespace pvpgn::infra::scripting
