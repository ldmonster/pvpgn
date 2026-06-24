// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/admin_commands/router.hpp"

#include <array>
#include <string_view>

namespace pvpgn::application::admin_commands {

namespace {

struct Alias {
    std::string_view alias;
    std::string_view canonical;
};

// Add an entry here when routing a new legacy command through this
// bridge.
constexpr std::array<Alias, 117> kAliases = {{
    {"/version", "/version"},
    {"/ver",     "/version"},
    {"/uptime",  "/uptime"},
    {"/help",    "/help"},
    {"/?",       "/help"},
    // Read-only info commands. Bodies still in legacy
    // command.cpp; the bridge calls the un-static'd handlers.
    {"/who",     "/who"},
    {"/whoami",  "/whoami"},
    {"/users",   "/users"},
    {"/finger",  "/finger"},
    // Read-only batch.
    {"/time",     "/time"},
    {"/news",     "/news"},
    {"/games",    "/games"},
    {"/channels", "/channels"},
    {"/motd",     "/motd"},
    // Server-info batch.
    {"/copyright",   "/copyright"},
    {"/lusers",      "/lusers"},
    {"/connections", "/connections"},
    {"/admins",      "/admins"},
    // Per-session state commands.
    {"/quit",      "/quit"},
    {"/beep",      "/beep"},
    {"/nobeep",    "/nobeep"},
    {"/away",      "/away"},
    {"/dnd",       "/dnd"},
    {"/squelch",   "/squelch"},
    {"/unsquelch", "/unsquelch"},
    // Social / messaging commands.
    {"/clan",       "/clan"},
    {"/c",          "/clan"},
    {"/friends",    "/friends"},
    {"/f",          "/friends"},
    {"/me",         "/me"},
    {"/emote",      "/me"},
    {"/whisper",    "/whisper"},
    {"/w",          "/whisper"},
    {"/m",          "/whisper"},
    {"/msg",        "/whisper"},
    {"/watch",      "/watch"},
    {"/unwatch",    "/unwatch"},
    {"/tos",        "/tos"},
    {"/clearstats", "/clearstats"},
    // Backfill aliases for previously-routed canonicals.
    {"/warranty",  "/copyright"},
    {"/license",   "/copyright"},
    {"/ignore",    "/squelch"},
    {"/unignore",  "/unsquelch"},
    {"/logout",    "/quit"},
    {"/exit",      "/quit"},
    {"/con",       "/connections"},
    {"/chs",       "/channels"},
    // Channel/chat-ops commands.
    {"/channel",    "/channel"},
    {"/join",       "/channel"},
    {"/j",          "/channel"},
    {"/rejoin",     "/rejoin"},
    {"/topic",      "/topic"},
    {"/moderate",   "/moderate"},
    {"/announce",   "/announce"},
    {"/ann",        "/announce"},
    {"/reply",      "/reply"},
    {"/r",          "/reply"},
    {"/realmann",   "/realmann"},
    {"/watchall",   "/watchall"},
    {"/unwatchall", "/unwatchall"},
    {"/alert",      "/alert"},
    // Channel rights / op commands.
    {"/admin",    "/admin"},
    {"/operator", "/operator"},
    {"/aop",      "/aop"},
    {"/op",       "/op"},
    {"/tmpop",    "/tmpop"},
    {"/deop",     "/deop"},
    {"/voice",    "/voice"},
    {"/devoice",  "/devoice"},
    {"/vop",      "/vop"},
    // Moderation / account-state commands.
    {"/kick",       "/kick"},
    {"/ban",        "/ban"},
    {"/unban",      "/unban"},
    {"/lockacct",   "/lockacct"},
    {"/lock",       "/lockacct"},
    {"/unlockacct", "/unlockacct"},
    {"/unlock",     "/unlockacct"},
    {"/muteacct",   "/muteacct"},
    {"/mute",       "/muteacct"},
    {"/unmuteacct", "/unmuteacct"},
    {"/unmute",     "/unmuteacct"},
    {"/flag",       "/flag"},
    {"/tag",        "/tag"},
    // Account / admin-ops commands.
    {"/addacct",   "/addacct"},
    {"/chpass",    "/chpass"},
    {"/kill",      "/kill"},
    {"/killsession", "/killsession"},
    {"/find",      "/find"},
    {"/save",      "/save"},
    {"/set",       "/set"},
    {"/rehash",    "/rehash"},
    {"/config",    "/config"},
    {"/shutdown",  "/shutdown"},
    {"/serverban", "/serverban"},
    // Info / network / misc commands.
    {"/stats",         "/stats"},
    {"/astat",         "/stats"},
    {"/whois",         "/whois"},
    {"/whereis",       "/whois"},
    {"/where",         "/whois"},
    {"/gameinfo",      "/gameinfo"},
    {"/ladderactivate", "/ladderactivate"},
    {"/ladderinfo",    "/ladderinfo"},
    {"/timer",         "/timer"},
    {"/netinfo",       "/netinfo"},
    {"/quota",         "/quota"},
    {"/ipscan",        "/ipscan"},
    {"/commandgroups", "/commandgroups"},
    {"/cg",            "/commandgroups"},
    {"/ping",          "/ping"},
    {"/p",             "/ping"},
    {"/latency",       "/ping"},
    // Extern bodies in other TUs.
    {"/mail",     "/mail"},
    {"/icon",     "/icon"},
    {"/ipban",    "/ipban"},
    {"/language", "/language"},
    {"/lang",     "/language"},
    {"/log",      "/log"},
}};

// Return the first whitespace-delimited token of `s`, lowercased.
// (Legacy command matching in `strstart` is case-insensitive.)
std::string_view first_token(std::string_view s) noexcept
{
    std::size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    const std::size_t start = i;
    while (i < s.size() && s[i] != ' ' && s[i] != '\t' && s[i] != '\r' && s[i] != '\n')
        ++i;
    return s.substr(start, i - start);
}

bool ieq(std::string_view a, std::string_view b) noexcept
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const char ca = a[i];
        const char cb = b[i];
        const char la = (ca >= 'A' && ca <= 'Z') ? char(ca + 32) : ca;
        const char lb = (cb >= 'A' && cb <= 'Z') ? char(cb + 32) : cb;
        if (la != lb) return false;
    }
    return true;
}

}  // namespace

RouteDecision
route(std::string_view command_line, const PermissionPredicate& is_permitted)
{
    const std::string_view tok = first_token(command_line);
    if (tok.empty() || tok.front() != '/') {
        return {RouteAction::NotFound, {}};
    }
    for (const Alias& a : kAliases) {
        if (ieq(tok, a.alias)) {
            const std::string canon{a.canonical};
            if (!is_permitted || !is_permitted(std::string_view{canon})) {
                return {RouteAction::Denied, canon};
            }
            return {RouteAction::Handled, canon};
        }
    }
    return {RouteAction::NotFound, {}};
}

}  // namespace pvpgn::application::admin_commands
