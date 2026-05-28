// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for the R216 legacy-command router (strangler-fig
// scaffold). The router itself is pure-v3 and decides whether an
// incoming legacy command line should be dispatched by the v3 bridge
// or fall back to legacy `handle_command`.

#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "application/admin_commands/router.hpp"

namespace ac = pvpgn::application::admin_commands;

namespace {

ac::PermissionPredicate always_allow()
{
    return [](std::string_view) { return true; };
}

ac::PermissionPredicate always_deny()
{
    return [](std::string_view) { return false; };
}

}  // namespace

TEST_CASE("router resolves /version and its /ver alias", "[admin_commands][router]")
{
    {
        const auto d = ac::route("/version", always_allow());
        REQUIRE(d.action == ac::RouteAction::Handled);
        REQUIRE(d.canonical_name == "/version");
    }
    {
        const auto d = ac::route("/ver", always_allow());
        REQUIRE(d.action == ac::RouteAction::Handled);
        REQUIRE(d.canonical_name == "/version");
    }
}

TEST_CASE("router resolves /uptime", "[admin_commands][router]")
{
    const auto d = ac::route("/uptime", always_allow());
    REQUIRE(d.action == ac::RouteAction::Handled);
    REQUIRE(d.canonical_name == "/uptime");
}

TEST_CASE("router resolves /help and its /? alias", "[admin_commands][router]")
{
    {
        const auto d = ac::route("/help", always_allow());
        REQUIRE(d.action == ac::RouteAction::Handled);
        REQUIRE(d.canonical_name == "/help");
    }
    {
        const auto d = ac::route("/?", always_allow());
        REQUIRE(d.action == ac::RouteAction::Handled);
        REQUIRE(d.canonical_name == "/help");
    }
}

TEST_CASE("R216b: router resolves the read-only info batch",
          "[admin_commands][router]")
{
    {
        const auto d = ac::route("/who #op", always_allow());
        REQUIRE(d.action == ac::RouteAction::Handled);
        REQUIRE(d.canonical_name == "/who");
    }
    {
        const auto d = ac::route("/whoami", always_allow());
        REQUIRE(d.action == ac::RouteAction::Handled);
        REQUIRE(d.canonical_name == "/whoami");
    }
    {
        const auto d = ac::route("/users", always_allow());
        REQUIRE(d.action == ac::RouteAction::Handled);
        REQUIRE(d.canonical_name == "/users");
    }
    {
        const auto d = ac::route("/finger alice", always_allow());
        REQUIRE(d.action == ac::RouteAction::Handled);
        REQUIRE(d.canonical_name == "/finger");
    }
}

TEST_CASE("R220: router resolves the next read-only batch",
          "[admin_commands][router]")
{
    for (const auto* name : {"/time", "/news", "/games", "/channels", "/motd"}) {
        const auto d = ac::route(name, always_allow());
        REQUIRE(d.action == ac::RouteAction::Handled);
        REQUIRE(d.canonical_name == name);
    }
}

TEST_CASE("R221: router resolves the server-info batch",
          "[admin_commands][router]")
{
    for (const auto* name : {"/copyright", "/lusers", "/connections", "/admins"}) {
        const auto d = ac::route(name, always_allow());
        REQUIRE(d.action == ac::RouteAction::Handled);
        REQUIRE(d.canonical_name == name);
    }
}

TEST_CASE("R222: router resolves the per-session state batch",
          "[admin_commands][router]")
{
    for (const auto* name : {"/quit", "/beep", "/nobeep", "/away", "/dnd", "/squelch", "/unsquelch"}) {
        const auto d = ac::route(name, always_allow());
        REQUIRE(d.action == ac::RouteAction::Handled);
        REQUIRE(d.canonical_name == name);
    }
}

TEST_CASE("R224: router resolves the channel/chat-ops batch",
          "[admin_commands][router]")
{
    struct Case { const char* input; const char* canonical; };
    constexpr Case cases[] = {
        {"/channel",     "/channel"},
        {"/join",        "/channel"},
        {"/j",           "/channel"},
        {"/rejoin",      "/rejoin"},
        {"/topic",       "/topic"},
        {"/moderate",    "/moderate"},
        {"/announce",    "/announce"},
        {"/ann",         "/announce"},
        {"/reply",       "/reply"},
        {"/r",           "/reply"},
        {"/realmann",    "/realmann"},
        {"/watchall",    "/watchall"},
        {"/unwatchall",  "/unwatchall"},
        {"/alert",       "/alert"},
    };
    for (const auto& kase : cases) {
        const auto d = ac::route(kase.input, always_allow());
        REQUIRE(d.action == ac::RouteAction::Handled);
        REQUIRE(d.canonical_name == kase.canonical);
    }
}

TEST_CASE("R224: backfill aliases route to existing canonicals",
          "[admin_commands][router]")
{
    struct Case { const char* input; const char* canonical; };
    constexpr Case cases[] = {
        {"/warranty", "/copyright"},
        {"/license",  "/copyright"},
        {"/ignore",   "/squelch"},
        {"/unignore", "/unsquelch"},
        {"/logout",   "/quit"},
        {"/exit",     "/quit"},
        {"/con",      "/connections"},
        {"/chs",      "/channels"},
    };
    for (const auto& kase : cases) {
        const auto d = ac::route(kase.input, always_allow());
        REQUIRE(d.action == ac::RouteAction::Handled);
        REQUIRE(d.canonical_name == kase.canonical);
    }
}


TEST_CASE("R223: router resolves the social/messaging batch",
          "[admin_commands][router]")
{
    struct Case { const char* input; const char* canonical; };
    constexpr Case cases[] = {
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
    };
    for (const auto& kase : cases) {
        const auto d = ac::route(kase.input, always_allow());
        REQUIRE(d.action == ac::RouteAction::Handled);
        REQUIRE(d.canonical_name == kase.canonical);
    }
}

TEST_CASE("R225: router resolves the channel rights / op batch",
          "[admin_commands][router]")
{
    for (const auto* name : {"/admin", "/operator", "/aop", "/op", "/tmpop", "/deop", "/voice", "/devoice", "/vop"}) {
        const auto d = ac::route(name, always_allow());
        REQUIRE(d.action == ac::RouteAction::Handled);
        REQUIRE(d.canonical_name == name);
    }
}

TEST_CASE("R229: router resolves the cross-TU bodies batch",
          "[admin_commands][router]")
{
    struct Case { const char* input; const char* canonical; };
    constexpr Case cases[] = {
        {"/mail",     "/mail"},
        {"/icon",     "/icon"},
        {"/ipban",    "/ipban"},
        {"/language", "/language"},
        {"/lang",     "/language"},
        {"/log",      "/log"},
    };
    for (const auto& kase : cases) {
        const auto d = ac::route(kase.input, always_allow());
        REQUIRE(d.action == ac::RouteAction::Handled);
        REQUIRE(d.canonical_name == kase.canonical);
    }
}

TEST_CASE("R228: router resolves the info / network / misc batch",
          "[admin_commands][router]")
{
    struct Case { const char* input; const char* canonical; };
    constexpr Case cases[] = {
        {"/stats",          "/stats"},
        {"/astat",          "/stats"},
        {"/whois",          "/whois"},
        {"/whereis",        "/whois"},
        {"/where",          "/whois"},
        {"/gameinfo",       "/gameinfo"},
        {"/ladderactivate", "/ladderactivate"},
        {"/ladderinfo",     "/ladderinfo"},
        {"/timer",          "/timer"},
        {"/netinfo",        "/netinfo"},
        {"/quota",          "/quota"},
        {"/ipscan",         "/ipscan"},
        {"/commandgroups",  "/commandgroups"},
        {"/cg",             "/commandgroups"},
        {"/ping",           "/ping"},
        {"/p",              "/ping"},
        {"/latency",        "/ping"},
    };
    for (const auto& kase : cases) {
        const auto d = ac::route(kase.input, always_allow());
        REQUIRE(d.action == ac::RouteAction::Handled);
        REQUIRE(d.canonical_name == kase.canonical);
    }
}

TEST_CASE("R227: router resolves the account / admin-ops batch",
          "[admin_commands][router]")
{
    for (const auto* name : {"/addacct", "/chpass", "/kill", "/killsession", "/find", "/save", "/set", "/rehash", "/config", "/shutdown", "/serverban"}) {
        const auto d = ac::route(name, always_allow());
        REQUIRE(d.action == ac::RouteAction::Handled);
        REQUIRE(d.canonical_name == name);
    }
}

TEST_CASE("R226: router resolves the moderation / account-state batch",
          "[admin_commands][router]")
{
    struct Case { const char* input; const char* canonical; };
    constexpr Case cases[] = {
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
    };
    for (const auto& kase : cases) {
        const auto d = ac::route(kase.input, always_allow());
        REQUIRE(d.action == ac::RouteAction::Handled);
        REQUIRE(d.canonical_name == kase.canonical);
    }
}

TEST_CASE("router ignores trailing arguments", "[admin_commands][router]")
{
    const auto d = ac::route("/version foo bar", always_allow());
    REQUIRE(d.action == ac::RouteAction::Handled);
    REQUIRE(d.canonical_name == "/version");
}

TEST_CASE("router skips leading whitespace", "[admin_commands][router]")
{
    const auto d = ac::route("   /uptime", always_allow());
    REQUIRE(d.action == ac::RouteAction::Handled);
    REQUIRE(d.canonical_name == "/uptime");
}

TEST_CASE("router is case insensitive on the command token", "[admin_commands][router]")
{
    const auto d = ac::route("/VERSION", always_allow());
    REQUIRE(d.action == ac::RouteAction::Handled);
    REQUIRE(d.canonical_name == "/version");
}

TEST_CASE("router returns NotFound for unrecognised commands", "[admin_commands][router]")
{
    // Pick something not in the router table.
    const auto d = ac::route("/notarealcommand foo", always_allow());
    REQUIRE(d.action == ac::RouteAction::NotFound);
    REQUIRE(d.canonical_name.empty());
}

TEST_CASE("router returns NotFound for non-slash text", "[admin_commands][router]")
{
    const auto d = ac::route("hello world", always_allow());
    REQUIRE(d.action == ac::RouteAction::NotFound);
}

TEST_CASE("router returns NotFound on empty input", "[admin_commands][router]")
{
    const auto d = ac::route("", always_allow());
    REQUIRE(d.action == ac::RouteAction::NotFound);
}

TEST_CASE("router returns Denied when predicate rejects", "[admin_commands][router]")
{
    const auto d = ac::route("/uptime", always_deny());
    REQUIRE(d.action == ac::RouteAction::Denied);
    REQUIRE(d.canonical_name == "/uptime");
}

TEST_CASE("router consults predicate with the canonical name, not the alias",
          "[admin_commands][router]")
{
    std::vector<std::string> queried;
    ac::PermissionPredicate spy = [&](std::string_view n) {
        queried.emplace_back(n);
        return true;
    };
    const auto d = ac::route("/ver", spy);
    REQUIRE(d.action == ac::RouteAction::Handled);
    REQUIRE(d.canonical_name == "/version");
    REQUIRE(queried.size() == 1);
    REQUIRE(queried.front() == "/version");
}
