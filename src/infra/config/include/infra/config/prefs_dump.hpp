// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file prefs_dump.hpp
/// Pure formatting helpers that render a `*LegacyPrefs` adapter as a
/// compact TOML-shaped listing (one field per line).
///
/// Used by:
///   - bnetd `/config` slash command (`src/bnetd/command.cpp`)
///   - d2cs / d2dbs SIGHUP-reload eventlog dump
///     (`src/d2cs/handle_signal.cpp`, `src/d2dbs/handle_signal.cpp`)
///   - Catch2 tests (`tests/unit/infra/config/prefs_dump_test.cpp`)
///
/// The output is intentionally **not** round-trip-safe TOML --
/// values are emitted bare (no escape sequences) so operators can
/// copy/paste paths and addresses straight into their config.

#include <string>
#include <string_view>
#include <vector>

#include "infra/config/d2cs_legacy_prefs.hpp"
#include "infra/config/d2dbs_legacy_prefs.hpp"
#include "infra/config/legacy_prefs.hpp"

namespace pvpgn::infra::config {

namespace prefs_dump_detail {

inline std::string kv_str(std::string_view key, std::string_view value) {
    std::string out;
    out.reserve(key.size() + value.size() + 6);
    out.append(key);
    out.append(" = \"");
    out.append(value);
    out.push_back('"');
    return out;
}

inline std::string kv_uint(std::string_view key, std::uint64_t value) {
    std::string out;
    out.append(key);
    out.append(" = ");
    out.append(std::to_string(value));
    return out;
}

inline std::string kv_bool(std::string_view key, bool value) {
    std::string out;
    out.append(key);
    out.append(value ? " = true" : " = false");
    return out;
}

}  // namespace prefs_dump_detail

/// Render the bnetd config snapshot.
inline std::vector<std::string> format_dump(const LegacyPrefs& p) {
    using namespace prefs_dump_detail;
    std::vector<std::string> out;
    out.reserve(20);

    out.emplace_back("[server]");
    out.emplace_back(kv_str("servername", p.servername()));
    out.emplace_back(kv_str("hostname",   p.hostname()));
    out.emplace_back("");

    out.emplace_back("[log]");
    out.emplace_back(kv_str("logfile",   p.logfile()));
    out.emplace_back(kv_str("loglevels", p.loglevels()));
    out.emplace_back("");

    out.emplace_back("[network]");
    out.emplace_back(kv_str("bnetd",   p.bnetdserv_addrs()));
    out.emplace_back(kv_str("telnet",  p.telnet_addrs()));
    out.emplace_back(kv_str("irc",     p.irc_addrs()));
    out.emplace_back(kv_str("w3route", p.w3route_addr()));
    out.emplace_back("");

    out.emplace_back("[files]");
    out.emplace_back(kv_str("filedir",   p.filedir()));
    out.emplace_back(kv_str("i18ndir",   p.i18ndir()));
    out.emplace_back(kv_str("storage",   p.storage_path()));
    out.emplace_back(kv_str("realmfile", p.realmfile()));
    return out;
}

/// Render the d2cs config snapshot.
inline std::vector<std::string> format_dump(const D2csLegacyPrefs& p) {
    using namespace prefs_dump_detail;
    std::vector<std::string> out;
    out.reserve(20);

    out.emplace_back("[server]");
    out.emplace_back(kv_str("realmname", p.realmname()));
    out.emplace_back("");

    out.emplace_back("[log]");
    out.emplace_back(kv_str("logfile",   p.logfile()));
    out.emplace_back(kv_str("loglevels", p.loglevels()));
    out.emplace_back("");

    out.emplace_back("[network]");
    out.emplace_back(kv_str("servaddrs",       p.servaddrs()));
    out.emplace_back(kv_str("bnetdaddr",       p.bnetdaddr()));
    out.emplace_back(kv_str("gameservlist",    p.gameservlist()));
    out.emplace_back(kv_uint("max_connections", p.max_connections()));
    out.emplace_back("");

    out.emplace_back("[files]");
    out.emplace_back(kv_str("charsave_dir", p.charsave_dir()));
    out.emplace_back(kv_str("charinfo_dir", p.charinfo_dir()));
    out.emplace_back(kv_str("ladder_dir",   p.ladder_dir()));
    out.emplace_back(kv_str("transfile",    p.transfile()));
    return out;
}

/// Render the d2dbs config snapshot.
inline std::vector<std::string> format_dump(const D2dbsLegacyPrefs& p) {
    using namespace prefs_dump_detail;
    std::vector<std::string> out;
    out.reserve(16);

    out.emplace_back("[log]");
    out.emplace_back(kv_str("logfile",    p.logfile()));
    out.emplace_back(kv_str("logfile_gs", p.logfile_gs()));
    out.emplace_back(kv_str("loglevels",  p.loglevels()));
    out.emplace_back("");

    out.emplace_back("[network]");
    out.emplace_back(kv_str("servaddrs",    p.servaddrs()));
    out.emplace_back(kv_str("gameservlist", p.gameservlist()));
    out.emplace_back("");

    out.emplace_back("[files]");
    out.emplace_back(kv_str("charsave_dir",     p.charsave_dir()));
    out.emplace_back(kv_str("charinfo_dir",     p.charinfo_dir()));
    out.emplace_back(kv_str("ladder_dir",       p.ladder_dir()));
    out.emplace_back(kv_str("bak_charsave_dir", p.bak_charsave_dir()));
    out.emplace_back(kv_str("bak_charinfo_dir", p.bak_charinfo_dir()));
    return out;
}

}  // namespace pvpgn::infra::config
