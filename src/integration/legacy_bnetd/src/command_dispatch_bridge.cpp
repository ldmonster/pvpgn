// SPDX-License-Identifier: GPL-2.0-or-later
//
// R216: strangler-fig dispatch entry for the legacy bnetd chat
// command pipeline. Bridge between legacy `handle_command()` in
// `src/bnetd/command.cpp` and the v3 router in
// `application/admin_commands/router.hpp`.
//
// First migration set: `/version` (alias `/ver`), `/uptime`,
// `/help` (alias `/?`).
//
// Contract of the exported `pvpgn_v3_command_dispatch` symbol:
//   return 0      -> v3 did not consume this command; legacy MUST
//                    fall through and dispatch it itself.
//   return non-0  -> v3 consumed the command (either the handler ran,
//                    or the caller was denied); legacy MUST stop and
//                    not run a second handler.
//
// /version and /uptime are reimplemented in pure v3 below (modulo
// i18n -- the v3 path has no i18n catalog yet, so the response text
// is English-only). /help still delegates straight into legacy
// `handle_help_command` because that handler does file I/O over the
// help corpus and rewriting it is not in scope for R216. The
// strangler step here is that v3 owns the dispatch decision, not the
// implementation of every command.

#include "integration/legacy_bnetd/command_dispatch_bridge.hpp"

#include <string>
#include <string_view>

#include "application/admin_commands/router.hpp"
#include "application/admin_commands/file_help_responder.hpp"  // R216f
#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "integration/legacy_bnetd/legacy_help_responder.hpp"  // R216d (kept; unused now)
#include "integration/legacy_bnetd/legacy_help_command_permissions.hpp"  // R216f
#include "integration/legacy_bnetd/legacy_help_corpus_provider.hpp"      // R216f
#include "integration/legacy_bnetd/legacy_message_sink.hpp"              // R216f

// Legacy headers -- gated with setup_before / setup_after as required
// by integration_legacy_bnetd_linked (only this lib may include them).
#include "common/setup_before.h"
#include "common/util.h"
#include "common/version.h"

#include "connection.h"
#include "account.h"
#include "account_wrap.h"
#include "command.h"
#include "command_groups.h"
#include "command_legacy.h"  // R216b: un-static'd legacy handlers
#include "i18n.h"            // R216c: legacy localize() macro
#include "message.h"
#include "server.h"
#include "userlog.h"
#include "common/setup_after.h"

namespace pvpgn::integration::legacy_bnetd {

namespace ac = pvpgn::application::admin_commands;

namespace {

using ::pvpgn::bnetd::t_account;
using ::pvpgn::bnetd::t_connection;
using ::pvpgn::bnetd::account_get_command_groups;
using ::pvpgn::bnetd::command_get_group;
using ::pvpgn::bnetd::conn_get_account;
using ::pvpgn::bnetd::_handle_status_command;
using ::pvpgn::bnetd::_handle_who_command;
using ::pvpgn::bnetd::_handle_whoami_command;
using ::pvpgn::bnetd::_handle_finger_command;
// R220:
using ::pvpgn::bnetd::_handle_time_command;
using ::pvpgn::bnetd::_handle_news_command;
using ::pvpgn::bnetd::_handle_games_command;
using ::pvpgn::bnetd::_handle_channels_command;
using ::pvpgn::bnetd::_handle_motd_command;
// R221:
using ::pvpgn::bnetd::_handle_copyright_command;
using ::pvpgn::bnetd::_handle_lusers_command;
using ::pvpgn::bnetd::_handle_connections_command;
using ::pvpgn::bnetd::_handle_admins_command;
// R222:
using ::pvpgn::bnetd::_handle_quit_command;
using ::pvpgn::bnetd::_handle_beep_command;
using ::pvpgn::bnetd::_handle_nobeep_command;
using ::pvpgn::bnetd::_handle_away_command;
using ::pvpgn::bnetd::_handle_dnd_command;
using ::pvpgn::bnetd::_handle_squelch_command;
using ::pvpgn::bnetd::_handle_unsquelch_command;
// R223:
using ::pvpgn::bnetd::_handle_clan_command;
using ::pvpgn::bnetd::_handle_friends_command;
using ::pvpgn::bnetd::_handle_me_command;
using ::pvpgn::bnetd::_handle_whisper_command;
using ::pvpgn::bnetd::_handle_watch_command;
using ::pvpgn::bnetd::_handle_unwatch_command;
using ::pvpgn::bnetd::_handle_tos_command;
using ::pvpgn::bnetd::_handle_clearstats_command;
// R224:
using ::pvpgn::bnetd::_handle_channel_command;
using ::pvpgn::bnetd::_handle_rejoin_command;
using ::pvpgn::bnetd::_handle_topic_command;
using ::pvpgn::bnetd::_handle_moderate_command;
using ::pvpgn::bnetd::_handle_announce_command;
using ::pvpgn::bnetd::_handle_reply_command;
using ::pvpgn::bnetd::_handle_realmann_command;
using ::pvpgn::bnetd::_handle_watchall_command;
using ::pvpgn::bnetd::_handle_unwatchall_command;
using ::pvpgn::bnetd::_handle_alert_command;
// R225:
using ::pvpgn::bnetd::_handle_admin_command;
using ::pvpgn::bnetd::_handle_operator_command;
using ::pvpgn::bnetd::_handle_aop_command;
using ::pvpgn::bnetd::_handle_op_command;
using ::pvpgn::bnetd::_handle_tmpop_command;
using ::pvpgn::bnetd::_handle_deop_command;
using ::pvpgn::bnetd::_handle_voice_command;
using ::pvpgn::bnetd::_handle_devoice_command;
using ::pvpgn::bnetd::_handle_vop_command;
// R226:
using ::pvpgn::bnetd::_handle_kick_command;
using ::pvpgn::bnetd::_handle_ban_command;
using ::pvpgn::bnetd::_handle_unban_command;
using ::pvpgn::bnetd::_handle_lockacct_command;
using ::pvpgn::bnetd::_handle_unlockacct_command;
using ::pvpgn::bnetd::_handle_muteacct_command;
using ::pvpgn::bnetd::_handle_unmuteacct_command;
using ::pvpgn::bnetd::_handle_flag_command;
using ::pvpgn::bnetd::_handle_tag_command;
// R227:
using ::pvpgn::bnetd::_handle_addacct_command;
using ::pvpgn::bnetd::_handle_chpass_command;
using ::pvpgn::bnetd::_handle_kill_command;
using ::pvpgn::bnetd::_handle_killsession_command;
using ::pvpgn::bnetd::_handle_find_command;
using ::pvpgn::bnetd::_handle_save_command;
using ::pvpgn::bnetd::_handle_set_command;
using ::pvpgn::bnetd::_handle_rehash_command;
using ::pvpgn::bnetd::_handle_config_command;
using ::pvpgn::bnetd::_handle_shutdown_command;
using ::pvpgn::bnetd::_handle_serverban_command;
// R228:
using ::pvpgn::bnetd::_handle_stats_command;
using ::pvpgn::bnetd::_handle_whois_command;
using ::pvpgn::bnetd::_handle_gameinfo_command;
using ::pvpgn::bnetd::_handle_ladderactivate_command;
using ::pvpgn::bnetd::_handle_ladderinfo_command;
using ::pvpgn::bnetd::_handle_timer_command;
using ::pvpgn::bnetd::_handle_netinfo_command;
using ::pvpgn::bnetd::_handle_quota_command;
using ::pvpgn::bnetd::_handle_ipscan_command;
using ::pvpgn::bnetd::_handle_commandgroups_command;
using ::pvpgn::bnetd::_handle_ping_command;
// R229: bodies in other legacy TUs.
using ::pvpgn::bnetd::handle_mail_command;
using ::pvpgn::bnetd::handle_icon_command;
using ::pvpgn::bnetd::handle_ipban_command;
using ::pvpgn::bnetd::handle_language_command;
using ::pvpgn::bnetd::handle_log_command;
using ::pvpgn::bnetd::message_send_text;
using ::pvpgn::bnetd::message_type_error;
using ::pvpgn::bnetd::message_type_info;
using ::pvpgn::bnetd::server_get_uptime;
using ::pvpgn::bnetd::userlog_append;
using ::pvpgn::seconds_to_timestr;

// Mirrors the per-command permission check from legacy
// `handle_command`:
//     command_get_group(p->command_string)
//       & account_get_command_groups(conn_get_account(c))
ac::PermissionPredicate make_predicate(t_connection* c)
{
    return [c](std::string_view canonical) {
        const std::string name{canonical};  // legacy API wants NUL-terminated
        const unsigned int needed = command_get_group(name.c_str());
        if (needed == 0) {
            // legacy interprets this as "command has been deactivated"
            return false;
        }
        t_account* acct = conn_get_account(c);
        const unsigned int have = (acct != nullptr)
            ? account_get_command_groups(acct)
            : 0u;
        return (needed & have) != 0;
    };
}

// Send the canonical denial message (deactivated vs. reserved) so the
// user sees exactly the same wording legacy `handle_command` uses.
// R216c: routed through the legacy localize() macro for full i18n
// parity with the original dispatch loop.
void send_denied(t_connection* c, std::string_view canonical)
{
    const std::string name{canonical};
    const unsigned int needed = command_get_group(name.c_str());
    if (needed == 0) {
        const std::string msg = localize(c, "This command has been deactivated");
        message_send_text(c, message_type_error, c, msg.c_str());
    } else {
        const std::string msg = localize(c, "This command is reserved for admins.");
        message_send_text(c, message_type_error, c, msg.c_str());
    }
}

// Run the v3 implementation for the canonical command name. Returns
// true on success; false makes the bridge fall through to legacy.
bool run_handler(t_connection* c, char const* text, std::string_view canonical)
{
    if (canonical == "/version") {
        // Identical wording to legacy `_handle_version_command`.
        // R216c: not localized because the legacy handler also
        // sent an untranslated literal here.
        message_send_text(c, message_type_info, c,
                          PVPGN_SOFTWARE " " PVPGN_VERSION);
        return true;
    }
    if (canonical == "/uptime") {
        // R216c: localized via the legacy i18n catalog. The macro
        // `localize` keys off the format-string literal so this
        // hits the same entries used by `_handle_uptime_command`.
        const std::string msg = localize(
            c, "Uptime: {}", seconds_to_timestr(server_get_uptime()));
        message_send_text(c, message_type_info, c, msg.c_str());
        return true;
    }
    if (canonical == "/help") {
        // R216f: pure-v3 help responder. The legacy adapters are
        // wired here so the application layer stays legacy-free.
        // First-call init of the corpus loader is `std::call_once`
        // inside `LegacyHelpCorpusProvider`.
        static const LegacyHelpCorpusProvider     kCorpora{};
        static const LegacyHelpCommandPermissions kPerms{};
        static const LegacyMessageSink            kSink{};
        static const ac::FileHelpResponder        kResponder{kCorpora, kPerms, kSink};
        return kResponder.respond(c, std::string_view{text});
    }
    // R216b: thin strangler shells over read-only info commands.
    // v3 owns dispatch; bodies still live in legacy command.cpp
    // (un-static'd so we can call them by linker symbol).
    if (canonical == "/who") {
        return _handle_who_command(c, text) == 0;
    }
    if (canonical == "/whoami") {
        return _handle_whoami_command(c, text) == 0;
    }
    if (canonical == "/users") {
        // legacy: /users == /status, dispatched to _handle_status_command.
        return _handle_status_command(c, text) == 0;
    }
    if (canonical == "/finger") {
        return _handle_finger_command(c, text) == 0;
    }
    // R220: next batch of read-only info commands as delegating shells.
    if (canonical == "/time")     return _handle_time_command(c, text)     == 0;
    if (canonical == "/news")     return _handle_news_command(c, text)     == 0;
    if (canonical == "/games")    return _handle_games_command(c, text)    == 0;
    if (canonical == "/channels") return _handle_channels_command(c, text) == 0;
    if (canonical == "/motd")     return _handle_motd_command(c, text)     == 0;
    // R221: server-info batch.
    if (canonical == "/copyright")   return _handle_copyright_command(c, text)   == 0;
    if (canonical == "/lusers")      return _handle_lusers_command(c, text)      == 0;
    if (canonical == "/connections") return _handle_connections_command(c, text) == 0;
    if (canonical == "/admins")      return _handle_admins_command(c, text)      == 0;
    // R222: per-session state commands.
    if (canonical == "/quit")      return _handle_quit_command(c, text)      == 0;
    if (canonical == "/beep")      return _handle_beep_command(c, text)      == 0;
    if (canonical == "/nobeep")    return _handle_nobeep_command(c, text)    == 0;
    if (canonical == "/away")      return _handle_away_command(c, text)      == 0;
    if (canonical == "/dnd")       return _handle_dnd_command(c, text)       == 0;
    if (canonical == "/squelch")   return _handle_squelch_command(c, text)   == 0;
    if (canonical == "/unsquelch") return _handle_unsquelch_command(c, text) == 0;
    // R223: social / messaging commands.
    if (canonical == "/clan")       return _handle_clan_command(c, text)       == 0;
    if (canonical == "/friends")    return _handle_friends_command(c, text)    == 0;
    if (canonical == "/me")         return _handle_me_command(c, text)         == 0;
    if (canonical == "/whisper")    return _handle_whisper_command(c, text)    == 0;
    if (canonical == "/watch")      return _handle_watch_command(c, text)      == 0;
    if (canonical == "/unwatch")    return _handle_unwatch_command(c, text)    == 0;
    if (canonical == "/tos")        return _handle_tos_command(c, text)        == 0;
    if (canonical == "/clearstats") return _handle_clearstats_command(c, text) == 0;
    // R224: channel/chat-ops commands.
    if (canonical == "/channel")     return _handle_channel_command(c, text)     == 0;
    if (canonical == "/rejoin")      return _handle_rejoin_command(c, text)      == 0;
    if (canonical == "/topic")       return _handle_topic_command(c, text)       == 0;
    if (canonical == "/moderate")    return _handle_moderate_command(c, text)    == 0;
    if (canonical == "/announce")    return _handle_announce_command(c, text)    == 0;
    if (canonical == "/reply")       return _handle_reply_command(c, text)       == 0;
    if (canonical == "/realmann")    return _handle_realmann_command(c, text)    == 0;
    if (canonical == "/watchall")    return _handle_watchall_command(c, text)    == 0;
    if (canonical == "/unwatchall")  return _handle_unwatchall_command(c, text)  == 0;
    if (canonical == "/alert")       return _handle_alert_command(c, text)       == 0;
    // R225: channel rights / op commands.
    if (canonical == "/admin")    return _handle_admin_command(c, text)    == 0;
    if (canonical == "/operator") return _handle_operator_command(c, text) == 0;
    if (canonical == "/aop")      return _handle_aop_command(c, text)      == 0;
    if (canonical == "/op")       return _handle_op_command(c, text)       == 0;
    if (canonical == "/tmpop")    return _handle_tmpop_command(c, text)    == 0;
    if (canonical == "/deop")     return _handle_deop_command(c, text)     == 0;
    if (canonical == "/voice")    return _handle_voice_command(c, text)    == 0;
    if (canonical == "/devoice")  return _handle_devoice_command(c, text)  == 0;
    if (canonical == "/vop")      return _handle_vop_command(c, text)      == 0;
    // R226: moderation / account-state commands.
    if (canonical == "/kick")        return _handle_kick_command(c, text)        == 0;
    if (canonical == "/ban")         return _handle_ban_command(c, text)         == 0;
    if (canonical == "/unban")       return _handle_unban_command(c, text)       == 0;
    if (canonical == "/lockacct")    return _handle_lockacct_command(c, text)    == 0;
    if (canonical == "/unlockacct")  return _handle_unlockacct_command(c, text)  == 0;
    if (canonical == "/muteacct")    return _handle_muteacct_command(c, text)    == 0;
    if (canonical == "/unmuteacct")  return _handle_unmuteacct_command(c, text)  == 0;
    if (canonical == "/flag")        return _handle_flag_command(c, text)        == 0;
    if (canonical == "/tag")         return _handle_tag_command(c, text)         == 0;
    // R227: account / admin-ops commands.
    if (canonical == "/addacct")     return _handle_addacct_command(c, text)     == 0;
    if (canonical == "/chpass")      return _handle_chpass_command(c, text)      == 0;
    if (canonical == "/kill")        return _handle_kill_command(c, text)        == 0;
    if (canonical == "/killsession") return _handle_killsession_command(c, text) == 0;
    if (canonical == "/find")        return _handle_find_command(c, text)        == 0;
    if (canonical == "/save")        return _handle_save_command(c, text)        == 0;
    if (canonical == "/set")         return _handle_set_command(c, text)         == 0;
    if (canonical == "/rehash")      return _handle_rehash_command(c, text)      == 0;
    if (canonical == "/config")      return _handle_config_command(c, text)      == 0;
    if (canonical == "/shutdown")    return _handle_shutdown_command(c, text)    == 0;
    if (canonical == "/serverban")   return _handle_serverban_command(c, text)   == 0;
    // R228: info / network / misc commands.
    if (canonical == "/stats")           return _handle_stats_command(c, text)           == 0;
    if (canonical == "/whois")           return _handle_whois_command(c, text)           == 0;
    if (canonical == "/gameinfo")        return _handle_gameinfo_command(c, text)        == 0;
    if (canonical == "/ladderactivate")  return _handle_ladderactivate_command(c, text)  == 0;
    if (canonical == "/ladderinfo")      return _handle_ladderinfo_command(c, text)      == 0;
    if (canonical == "/timer")           return _handle_timer_command(c, text)           == 0;
    if (canonical == "/netinfo")         return _handle_netinfo_command(c, text)         == 0;
    if (canonical == "/quota")           return _handle_quota_command(c, text)           == 0;
    if (canonical == "/ipscan")          return _handle_ipscan_command(c, text)          == 0;
    if (canonical == "/commandgroups")   return _handle_commandgroups_command(c, text)   == 0;
    if (canonical == "/ping")            return _handle_ping_command(c, text)            == 0;
    // R229: extern bodies in other legacy TUs.
    if (canonical == "/mail")     return handle_mail_command(c, text)     == 0;
    if (canonical == "/icon")     return handle_icon_command(c, text)     == 0;
    if (canonical == "/ipban")    return handle_ipban_command(c, text)    == 0;
    if (canonical == "/language") return handle_language_command(c, text) == 0;
    if (canonical == "/log")      return handle_log_command(c, text)      == 0;
    return false;
}

void log_bridge(std::string_view event, std::string_view command)
{
    const pvpgn::core::ILogger::Field fields[] = {{"op", command}};
    bridge_log_kv(pvpgn::core::LogLevel::Debug,
                  "v3_command_dispatch_bridge",
                  event,
                  {fields[0]});
}

}  // namespace

}  // namespace pvpgn::integration::legacy_bnetd

extern "C" int pvpgn_v3_command_dispatch(void* conn_ptr, char const* op) noexcept {
    using namespace pvpgn::integration::legacy_bnetd;
    namespace ac = pvpgn::application::admin_commands;

    if (conn_ptr == nullptr || op == nullptr) {
        return 0;
    }
    auto* c = static_cast<::pvpgn::bnetd::t_connection*>(conn_ptr);
    const std::string_view text_sv{op};

    const ac::PermissionPredicate predicate = make_predicate(c);
    const ac::RouteDecision       decision  = ac::route(text_sv, predicate);

    switch (decision.action) {
    case ac::RouteAction::NotFound:
        log_bridge("command not in v3 set; deferring to legacy", text_sv);
        return 0;

    case ac::RouteAction::Denied:
        log_bridge("command denied by permission predicate",
                   decision.canonical_name);
        send_denied(c, decision.canonical_name);
        return 1;

    case ac::RouteAction::Handled: {
        const bool ok = run_handler(c, op, decision.canonical_name);
        if (!ok) {
            log_bridge("handler returned failure; falling back to legacy",
                       decision.canonical_name);
            return 0;
        }
        log_bridge("command handled by v3", decision.canonical_name);
        if (auto* acct = ::pvpgn::bnetd::conn_get_account(c)) {
            ::pvpgn::bnetd::userlog_append(acct, op);
        }
        return 1;
    }
    }
    return 0;  // unreachable
}

