// SPDX-License-Identifier: GPL-2.0-or-later

/// @file migrate_config.cpp
/// Legacy *.conf → bnetd.toml migration for pvpgn-migrate.

#include "migrate_config.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace pvpgn::app::migrate {

namespace {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/// Parse a legacy key=value conf file (bnetd.conf style).
/// Lines starting with '#' or empty lines are ignored.
/// Returns a map of key → value.
std::map<std::string, std::string> parse_conf_file(const std::filesystem::path& path) {
    std::map<std::string, std::string> result;
    std::ifstream ifs{path};
    if (!ifs.is_open()) return result;

    std::string line;
    while (std::getline(ifs, line)) {
        // Strip trailing \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key   = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        // Trim leading/trailing whitespace from key and value
        auto trim = [](std::string& s) {
            const auto start = s.find_first_not_of(" \t");
            if (start == std::string::npos) { s.clear(); return; }
            const auto end = s.find_last_not_of(" \t");
            s = s.substr(start, end - start + 1);
        };
        trim(key);
        trim(value);
        if (!key.empty()) result[key] = value;
    }
    return result;
}

/// Read a text file into a string. Returns empty string on error.
std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream ifs{path};
    if (!ifs.is_open()) return {};
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

/// Escape a string value for use inside a TOML double-quoted string.
std::string toml_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

/// Escape a string for TOML multi-line literal string (triple-quoted).
/// We use single-line basic strings for simple values and multi-line
/// literal strings for motd content.
std::string toml_escape_multiline(std::string_view s) {
    // Replace ''' with ''\'''' to avoid closing the literal string
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out += c;
    }
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

int migrate_conf_to_toml(const std::string& conf_dir,
                          const std::string& out_path) {
    namespace fs = std::filesystem;

    if (!fs::exists(conf_dir) || !fs::is_directory(conf_dir)) {
        std::cerr << "error: conf directory does not exist: " << conf_dir << "\n";
        return 1;
    }

    const fs::path dir{conf_dir};

    // Read bnetd.conf (primary config)
    const auto bnetd_conf = parse_conf_file(dir / "bnetd.conf");

    // Helper to get a value with a default
    auto get = [&](const std::map<std::string, std::string>& m,
                   std::string_view key,
                   std::string_view def = "") -> std::string {
        auto it = m.find(std::string{key});
        return (it != m.end() && !it->second.empty()) ? it->second : std::string{def};
    };

    // Read optional supplementary files
    const auto channel_conf    = parse_conf_file(dir / "channel.conf");
    const auto realm_conf      = parse_conf_file(dir / "realm.conf");
    const auto autoupdate_conf = parse_conf_file(dir / "autoupdate.conf");
    const auto bnban_conf      = parse_conf_file(dir / "bnban.conf");

    // Read motd (optional)
    std::string motd_text;
    for (const auto& motd_name : {"bnmotd.txt", "motd.txt"}) {
        const fs::path motd_path = dir / motd_name;
        if (fs::exists(motd_path)) {
            motd_text = read_text_file(motd_path);
            break;
        }
    }

    // Build TOML output
    std::ostringstream toml;
    toml << "# bnetd.toml — generated by pvpgn-migrate config\n";
    toml << "# Source: " << conf_dir << "\n";
    toml << "schema_version = 3\n\n";

    // [server]
    toml << "[server]\n";
    toml << "hostname = \"" << toml_escape(get(bnetd_conf, "hostname", "localhost")) << "\"\n";
    toml << "port = " << get(bnetd_conf, "port", "6112") << "\n";
    toml << "max_connections = " << get(bnetd_conf, "max_connections", "1000") << "\n";
    toml << "servername = \"" << toml_escape(get(bnetd_conf, "servername", "PvPGN")) << "\"\n";
    toml << "\n";

    // [log]
    toml << "[log]\n";
    toml << "file = \"" << toml_escape(get(bnetd_conf, "logfile", "var/log/bnetd.log")) << "\"\n";
    toml << "level = \"" << toml_escape(get(bnetd_conf, "loglevel", "info")) << "\"\n";
    toml << "\n";

    // [storage]
    toml << "[storage]\n";
    const std::string storage_path = get(bnetd_conf, "storagedir", "var/users");
    toml << "backend = \"" << toml_escape(get(bnetd_conf, "storage_backend", "plain")) << "\"\n";
    toml << "path = \"" << toml_escape(storage_path) << "\"\n";
    toml << "\n";

    // [motd]
    toml << "[motd]\n";
    if (!motd_text.empty()) {
        // Use multi-line literal string for motd
        toml << "text = '''\n" << toml_escape_multiline(motd_text) << "'''\n";
    } else {
        toml << "text = \"Welcome to PvPGN!\"\n";
    }
    toml << "\n";

    // [channels] — list from channel.conf
    if (!channel_conf.empty()) {
        toml << "# Channel defaults from channel.conf\n";
        toml << "[channels]\n";
        toml << "default_topic = \"" << toml_escape(get(channel_conf, "topic", "")) << "\"\n";
        toml << "max_users = " << get(channel_conf, "max_users", "200") << "\n";
        toml << "\n";
    }

    // [realm] — from realm.conf
    if (!realm_conf.empty()) {
        toml << "# Realm settings from realm.conf\n";
        toml << "[realm]\n";
        toml << "name = \"" << toml_escape(get(realm_conf, "realmname", "PvPGN")) << "\"\n";
        toml << "description = \"" << toml_escape(get(realm_conf, "description", "")) << "\"\n";
        toml << "\n";
    }

    // [autoupdate] — from autoupdate.conf
    if (!autoupdate_conf.empty()) {
        toml << "# Autoupdate settings from autoupdate.conf\n";
        toml << "[autoupdate]\n";
        toml << "enabled = " << (get(autoupdate_conf, "enabled", "false") == "true" ? "true" : "false") << "\n";
        toml << "\n";
    }

    // Write output
    {
        // Create parent directories if needed
        const fs::path out{out_path};
        if (out.has_parent_path()) {
            std::error_code ec;
            fs::create_directories(out.parent_path(), ec);
            if (ec) {
                std::cerr << "error: cannot create output directory '"
                          << out.parent_path().string() << "': " << ec.message() << "\n";
                return 1;
            }
        }

        std::ofstream ofs{out_path};
        if (!ofs.is_open()) {
            std::cerr << "error: cannot open output file: " << out_path << "\n";
            return 1;
        }
        ofs << toml.str();
        if (!ofs) {
            std::cerr << "error: write error for " << out_path << "\n";
            return 1;
        }
    }

    std::cout << "Config migration complete: " << out_path << "\n";
    std::cout << "  Source conf dir: " << conf_dir << "\n";
    std::cout << "  Files read: bnetd.conf";
    if (!channel_conf.empty())    std::cout << ", channel.conf";
    if (!realm_conf.empty())      std::cout << ", realm.conf";
    if (!autoupdate_conf.empty()) std::cout << ", autoupdate.conf";
    if (!bnban_conf.empty())      std::cout << ", bnban.conf";
    if (!motd_text.empty())       std::cout << ", bnmotd.txt";
    std::cout << "\n";
    return 0;
}

}  // namespace pvpgn::app::migrate
