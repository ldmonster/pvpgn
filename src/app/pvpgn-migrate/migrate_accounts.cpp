// SPDX-License-Identifier: GPL-2.0-or-later

/// @file migrate_accounts.cpp
/// Account migration functions for pvpgn-migrate.

#include "migrate_accounts.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

// v3 file-based persistence
#include "infra/file/flat_db_reader.hpp"

// v3 SQLite persistence
#include "infra/sqlite/unit_of_work_factory.hpp"

// Application ports (needed for full IAccountRepository definition)
#include "application/ports/account_repository.hpp"
#include "application/ports/unit_of_work.hpp"

// Domain
#include "domain/identity/account.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"

namespace pvpgn::app::migrate {

namespace {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/// Decode a 40-character hex string into a BNHash.
/// Returns a zeroed BNHash on any parse error.
pvpgn::domain::BNHash bn_hash_from_hex(std::string_view hex) {
    if (hex.size() != 40) {
        return pvpgn::domain::BNHash{};
    }
    pvpgn::domain::BNHash::Bytes bytes{};
    for (std::size_t i = 0; i < 20; ++i) {
        unsigned int hi = 0, lo = 0;
        const auto h = static_cast<unsigned char>(hex[i * 2]);
        const auto l = static_cast<unsigned char>(hex[i * 2 + 1]);
        if      (h >= '0' && h <= '9') hi = h - '0';
        else if (h >= 'a' && h <= 'f') hi = h - 'a' + 10;
        else if (h >= 'A' && h <= 'F') hi = h - 'A' + 10;
        else return pvpgn::domain::BNHash{};
        if      (l >= '0' && l <= '9') lo = l - '0';
        else if (l >= 'a' && l <= 'f') lo = l - 'a' + 10;
        else if (l >= 'A' && l <= 'F') lo = l - 'A' + 10;
        else return pvpgn::domain::BNHash{};
        bytes[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return pvpgn::domain::BNHash{bytes};
}

/// Parse a single *.plain file into an Account.
/// Returns nullopt if the file is invalid or unreadable.
std::optional<pvpgn::domain::identity::Account>
parse_plain_file(const std::filesystem::path& path) {
    std::ifstream ifs{path};
    if (!ifs.is_open()) {
        return std::nullopt;
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    const std::string content = oss.str();

    const auto kv = pvpgn::infra::file::parse_account_file(content);

    // Required: username
    const std::string username_str =
        pvpgn::infra::file::get_field(kv, {"BNET", "acct", "username"});
    if (username_str.empty()) {
        return std::nullopt;
    }

    auto name_result = pvpgn::domain::UserName::parse(username_str);
    if (!name_result.has_value()) {
        return std::nullopt;
    }

    // Account ID
    const auto uid_raw =
        pvpgn::infra::file::get_numeric_field(kv, {"BNET", "acct", "userid"});
    const auto account_id = pvpgn::domain::AccountId{
        static_cast<std::uint32_t>(uid_raw > 0 ? uid_raw : 0)};

    // Password hash
    const std::string hash_hex =
        pvpgn::infra::file::get_field(kv, {"BNET", "acct", "passhash1"});
    const pvpgn::domain::BNHash password = bn_hash_from_hex(hash_hex);

    // Locale
    const std::string locale_str =
        pvpgn::infra::file::get_field(kv, {"BNET", "acct", "locale"});
    const auto locale = pvpgn::domain::Locale::parse_or_default(locale_str);

    // Command groups
    pvpgn::domain::identity::CommandGroupMask groups;
    const auto cg_raw =
        pvpgn::infra::file::get_numeric_field(kv, {"BNET", "acct", "auth_command_groups"});
    if (cg_raw > 0) {
        for (std::uint8_t g = 1;
             g <= pvpgn::domain::identity::CommandGroupMask::kBits; ++g) {
            if (cg_raw & (1 << (g - 1))) groups.grant(g);
        }
    } else {
        groups.grant(1);
    }

    // Locked flag
    const auto lock_raw =
        pvpgn::infra::file::get_numeric_field(kv, {"BNET", "acct", "auth_lock"});
    const bool locked = (lock_raw != 0);

    return pvpgn::domain::identity::Account::rehydrate(
        account_id,
        std::move(name_result.value()),
        password,
        locale,
        groups,
        std::nullopt,
        locked,
        false);
}

/// Escape a string value for use inside a TOML double-quoted string.
std::string toml_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char raw : s) {
        const unsigned char c = static_cast<unsigned char>(raw);
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

}  // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

int migrate_plain_to_sqlite(const std::string& src_dir,
                              const std::string& db_path) {
    namespace fs = std::filesystem;

    // Validate source directory
    if (!fs::exists(src_dir) || !fs::is_directory(src_dir)) {
        std::cerr << "error: source directory does not exist: " << src_dir << "\n";
        return 1;
    }

    // Open / create SQLite database via factory (runs schema migrations)
    std::cout << "Opening SQLite database: " << db_path << "\n";
    pvpgn::infra::sqlite::SQLiteUnitOfWorkFactory factory{db_path};

    // Create a unit of work to access the account repository
    auto uow = factory.create();
    auto& repo = uow->accounts();

    // Begin transaction for bulk insert
    auto begin_result = uow->begin();
    if (!begin_result.has_value()) {
        std::cerr << "error: failed to begin transaction: "
                  << begin_result.error().message() << "\n";
        return 1;
    }

    std::size_t migrated = 0;
    std::size_t skipped  = 0;

    for (const auto& entry : fs::directory_iterator(src_dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".plain") continue;

        const std::string filename = entry.path().filename().string();

        auto account_opt = parse_plain_file(entry.path());
        if (!account_opt) {
            std::cerr << "warning: failed to parse " << filename << " — skipping\n";
            ++skipped;
            continue;
        }

        const std::string username{account_opt->name().display()};

        auto status = repo.save(*account_opt);
        if (!status.has_value()) {
            std::cerr << "warning: failed to save '" << username
                      << "': " << status.error().message() << " — skipping\n";
            ++skipped;
            continue;
        }

        std::cout << "Migrated: " << username << "\n";
        ++migrated;
    }

    // Commit transaction
    auto commit_result = uow->commit();
    if (!commit_result.has_value()) {
        std::cerr << "error: failed to commit transaction: "
                  << commit_result.error().message() << "\n";
        uow->rollback();
        return 1;
    }

    std::cout << "Migration complete: " << migrated << " accounts migrated";
    if (skipped > 0) {
        std::cout << " (" << skipped << " skipped)";
    }
    std::cout << "\n";
    return 0;
}

int migrate_plain_to_toml(const std::string& src_dir,
                            const std::string& dst_dir) {
    namespace fs = std::filesystem;

    // Validate source directory
    if (!fs::exists(src_dir) || !fs::is_directory(src_dir)) {
        std::cerr << "error: source directory does not exist: " << src_dir << "\n";
        return 1;
    }

    // Create destination directory if needed
    {
        std::error_code ec;
        fs::create_directories(dst_dir, ec);
        if (ec) {
            std::cerr << "error: cannot create destination directory '"
                      << dst_dir << "': " << ec.message() << "\n";
            return 1;
        }
    }

    std::size_t converted = 0;
    std::size_t skipped   = 0;

    for (const auto& entry : fs::directory_iterator(src_dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".plain") continue;

        const std::string filename = entry.path().filename().string();

        // Read file content for raw field extraction
        std::ifstream ifs{entry.path()};
        if (!ifs.is_open()) {
            std::cerr << "warning: cannot open " << entry.path() << " — skipping\n";
            ++skipped;
            continue;
        }
        std::ostringstream oss;
        oss << ifs.rdbuf();
        const std::string content = oss.str();

        const auto kv = pvpgn::infra::file::parse_account_file(content);

        // Required: username
        const std::string username_str =
            pvpgn::infra::file::get_field(kv, {"BNET", "acct", "username"});
        if (username_str.empty()) {
            std::cerr << "warning: no username in " << filename << " — skipping\n";
            ++skipped;
            continue;
        }

        auto name_result = pvpgn::domain::UserName::parse(username_str);
        if (!name_result.has_value()) {
            std::cerr << "warning: invalid username '" << username_str
                      << "' in " << filename << " — skipping\n";
            ++skipped;
            continue;
        }

        // Extract fields
        const std::string passhash1 =
            pvpgn::infra::file::get_field(kv, {"BNET", "acct", "passhash1"});
        const std::string email =
            pvpgn::infra::file::get_field(kv, {"BNET", "acct", "email"});
        const auto userid =
            pvpgn::infra::file::get_numeric_field(kv, {"BNET", "acct", "userid"});
        const auto flags =
            pvpgn::infra::file::get_numeric_field(kv, {"BNET", "acct", "auth_command_groups"});
        const auto created_at =
            pvpgn::infra::file::get_numeric_field(kv, {"BNET", "acct", "created"});
        const auto last_login =
            pvpgn::infra::file::get_numeric_field(kv, {"BNET", "acct", "lastlogin_time"});
        const std::string last_login_ip =
            pvpgn::infra::file::get_field(kv, {"BNET", "acct", "lastlogin_ip"});

        // Build TOML content
        std::ostringstream toml;
        toml << "[account]\n";
        toml << "username = \"" << toml_escape(username_str) << "\"\n";
        toml << "passhash1 = \"" << toml_escape(passhash1) << "\"\n";
        toml << "email = \"" << toml_escape(email) << "\"\n";
        toml << "userid = " << userid << "\n";
        toml << "flags = " << flags << "\n";
        toml << "\n";
        toml << "[timestamps]\n";
        toml << "created_at = " << created_at << "\n";
        toml << "last_login = " << last_login << "\n";
        toml << "\n";
        toml << "[network]\n";
        toml << "last_login_ip = \"" << toml_escape(last_login_ip) << "\"\n";

        // Write output file
        const fs::path out_path =
            fs::path{dst_dir} / (username_str + ".toml");
        std::ofstream ofs{out_path};
        if (!ofs.is_open()) {
            std::cerr << "warning: cannot write " << out_path << " — skipping\n";
            ++skipped;
            continue;
        }
        ofs << toml.str();
        if (!ofs) {
            std::cerr << "warning: write error for " << out_path << " — skipping\n";
            ++skipped;
            continue;
        }

        std::cout << "Converted: " << username_str << "\n";
        ++converted;
    }

    std::cout << "Conversion complete: " << converted << " accounts converted";
    if (skipped > 0) {
        std::cout << " (" << skipped << " skipped)";
    }
    std::cout << "\n";
    return 0;
}

}  // namespace pvpgn::app::migrate
