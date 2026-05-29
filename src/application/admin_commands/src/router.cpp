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

// R216 first migration set, extended in R216b and R220. Add an entry
// here when migrating a new legacy command into the v3 strangler-fig
// path.
constexpr std::array<Alias, 117> kAliases = {{
    {"/version", "/version"},
    {"/ver",     "/version"},
    {"/uptime",  "/uptime"},
    {"/help",    "/help"},
    {"/?",       "/help"},
    // R216b: read-only info commands. Bodies still in legacy
    // command.cpp; the v3 bridge calls the un-static'd handlers.
    {"/who",     "/who"},
    {"/whoami",  "/whoami"},
    {"/users",   "/users"},
    {"/finger",  "/finger"},
    // R220: next read-only batch.
    {"/time",     "/time"},
    {"/news",     "/news"},
    {"/games",    "/games"},
    {"/channels", "/channels"},
    {"/motd",     "/motd"},
    // R221: server-info batch.
    {"/copyright",   "/copyright"},
    {"/lusers",      "/lusers"},
    {"/connections", "/connections"},
    {"/admins",      "/admins"},
    // R222: per-session state commands.
    {"/quit",      "/quit"},
    {"/beep",      "/beep"},
    {"/nobeep",    "/nobeep"},
    {"/away",      "/away"},
    {"/dnd",       "/dnd"},
    {"/squelch",   "/squelch"},
    {"/unsquelch", "/unsquelch"},
    // R223: social / messaging commands.
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
    // R224: backfill aliases for previously-routed canonicals.
    {"/warranty",  "/copyright"},
    {"/license",   "/copyright"},
    {"/ignore",    "/squelch"},
    {"/unignore",  "/unsquelch"},
    {"/logout",    "/quit"},
    {"/exit",      "/quit"},
    {"/con",       "/connections"},
    {"/chs",       "/channels"},
    // R224: channel/chat-ops commands.
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
    // R225: channel rights / op commands.
    {"/admin",    "/admin"},
    {"/operator", "/operator"},
    {"/aop",      "/aop"},
    {"/op",       "/op"},
    {"/tmpop",    "/tmpop"},
    {"/deop",     "/deop"},
    {"/voice",    "/voice"},
    {"/devoice",  "/devoice"},
    {"/vop",      "/vop"},
    // R226: moderation / account-state commands.
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
    // R227: account / admin-ops commands.
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
    // R228: info / network / misc commands.
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
    // R229: extern bodies in other TUs.
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
