// SPDX-License-Identifier: GPL-2.0-or-later

/// @file account_repository.cpp
/// MySQL-backed IAccountRepository implementation.
///
/// Compiled only when PVPGN_V3_WITH_MYSQL is defined.
/// SQL schema mirrors the SQLite schema (same column names, compatible types).

#include "infra/mysql/account_repository.hpp"

#ifdef PVPGN_V3_WITH_MYSQL

#include <sstream>
#include <stdexcept>
#include <string>

#include "domain/identity/account.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"

namespace pvpgn::infra::mysql {

namespace {

/// Decode a 40-character hex string into a BNHash.
domain::BNHash bn_hash_from_hex(std::string_view hex) {
    if (hex.size() != 40) return domain::BNHash{};
    domain::BNHash::Bytes bytes{};
    for (std::size_t i = 0; i < 20; ++i) {
        unsigned int hi = 0, lo = 0;
        const auto h = static_cast<unsigned char>(hex[i * 2]);
        const auto l = static_cast<unsigned char>(hex[i * 2 + 1]);
        if (h >= '0' && h <= '9') hi = h - '0';
        else if (h >= 'a' && h <= 'f') hi = h - 'a' + 10;
        else if (h >= 'A' && h <= 'F') hi = h - 'A' + 10;
        else return domain::BNHash{};
        if (l >= '0' && l <= '9') lo = l - '0';
        else if (l >= 'a' && l <= 'f') lo = l - 'a' + 10;
        else if (l >= 'A' && l <= 'F') lo = l - 'A' + 10;
        else return domain::BNHash{};
        bytes[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return domain::BNHash{bytes};
}

/// Encode a BNHash to a 40-character lowercase hex string.
std::string bn_hash_to_hex(const domain::BNHash& hash) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(40);
    for (auto b : hash.bytes()) {
        out.push_back(kHex[(b >> 4) & 0xF]);
        out.push_back(kHex[b & 0xF]);
    }
    return out;
}

/// Build an Account from a query Row.
/// Column order: id, name, locale, password_hash, locked,
///               must_change_password, command_groups, created_at, updated_at
domain::identity::Account account_from_row(const Row& row) {
    const auto id = domain::AccountId{
        static_cast<std::uint32_t>(row.get_int(0))};

    auto name_result = domain::UserName::parse(row.get_text(1));
    auto name = name_result.has_value()
                    ? std::move(name_result.value())
                    : domain::UserName::parse("unknown").value();

    const domain::Locale locale =
        domain::Locale::parse_or_default(row.get_text(2));

    const domain::BNHash password = bn_hash_from_hex(row.get_text(3));

    const bool locked               = row.get_int(4) != 0;
    const bool must_change_password = row.get_int(5) != 0;

    domain::identity::CommandGroupMask groups;
    const std::string cg_str = row.get_text(6);
    if (!cg_str.empty()) {
        std::istringstream iss(cg_str);
        std::string token;
        while (std::getline(iss, token, ',')) {
            if (!token.empty()) {
                try {
                    const auto g = static_cast<std::uint8_t>(std::stoul(token));
                    groups.grant(g);
                } catch (...) {}
            }
        }
    }

    return domain::identity::Account::rehydrate(
        id, std::move(name), password, locale, groups,
        std::nullopt, locked, must_change_password);
}

/// Escape a string for safe embedding in SQL (single-quote doubling).
/// For production use, prefer parameterised queries via mysql_real_escape_string.
std::string sql_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '\'') out += "''";
        else out += c;
    }
    return out;
}

}  // namespace

MySQLAccountRepository::MySQLAccountRepository(
    std::shared_ptr<MySQLConnection> conn)
    : conn_(std::move(conn)) {}

core::Result<domain::identity::Account>
MySQLAccountRepository::find_by_id(domain::AccountId id) const {
    if (!conn_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "mysql: connection not available"});
    }

    domain::identity::Account* result = nullptr;

    std::ostringstream sql;
    sql << "SELECT id, name, locale, password_hash, locked, "
           "must_change_password, command_groups, created_at, updated_at "
           "FROM accounts WHERE id = "
        << id.value()
        << " LIMIT 1";

    auto qr = conn_->query(sql.str(), [&result](const Row& row) {
        try {
            result = new domain::identity::Account{account_from_row(row)};
        } catch (...) {}
        return false;
    });

    if (!qr.has_value()) return core::fail(qr.error());
    if (!result) {
        return core::fail(core::Error{core::StatusCode::NotFound,
                                      "mysql: account id not found"});
    }
    auto account = *result;
    delete result;
    return account;
}

core::Result<domain::identity::Account>
MySQLAccountRepository::find_by_name(const domain::UserName& name) const {
    if (!conn_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "mysql: connection not available"});
    }

    domain::identity::Account* result = nullptr;

    std::ostringstream sql;
    sql << "SELECT id, name, locale, password_hash, locked, "
           "must_change_password, command_groups, created_at, updated_at "
           "FROM accounts WHERE LOWER(name) = LOWER('"
        << sql_escape(name.canonical())
        << "') LIMIT 1";

    auto qr = conn_->query(sql.str(), [&result](const Row& row) {
        try {
            result = new domain::identity::Account{account_from_row(row)};
        } catch (...) {}
        return false;
    });

    if (!qr.has_value()) return core::fail(qr.error());
    if (!result) {
        return core::fail(core::Error{core::StatusCode::NotFound,
                                      "mysql: account name not found"});
    }
    auto account = *result;
    delete result;
    return account;
}

core::Status<> MySQLAccountRepository::save(
    const domain::identity::Account& account) {
    if (!conn_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "mysql: connection not available"});
    }

    // Build command groups string (comma-separated group numbers 1..8)
    std::ostringstream groups_oss;
    bool first_group = true;
    for (std::uint8_t g = 1; g <= domain::identity::CommandGroupMask::kBits; ++g) {
        if (account.command_groups().has(g)) {
            if (!first_group) groups_oss << ",";
            groups_oss << static_cast<int>(g);
            first_group = false;
        }
    }

    // INSERT ... ON DUPLICATE KEY UPDATE (MySQL upsert)
    std::ostringstream sql;
    sql << "INSERT INTO accounts "
           "(id, name, locale, password_hash, locked, must_change_password, "
           "command_groups, created_at, updated_at) VALUES ("
        << account.id().value() << ", '"
        << sql_escape(account.name().display()) << "', '"
        << sql_escape(account.locale().text()) << "', '"
        << bn_hash_to_hex(account.password_hash1()) << "', "
        << (account.is_locked() ? 1 : 0) << ", "
        << (account.must_change_password() ? 1 : 0) << ", '"
        << groups_oss.str() << "', 0, 0) "
        "ON DUPLICATE KEY UPDATE "
        "name = VALUES(name), locale = VALUES(locale), "
        "password_hash = VALUES(password_hash), locked = VALUES(locked), "
        "must_change_password = VALUES(must_change_password), "
        "command_groups = VALUES(command_groups), updated_at = 0";

    return conn_->exec(sql.str());
}

core::Status<> MySQLAccountRepository::remove(domain::AccountId id) {
    if (!conn_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "mysql: connection not available"});
    }
    std::ostringstream sql;
    sql << "DELETE FROM accounts WHERE id = " << id.value();
    return conn_->exec(sql.str());
}

void MySQLAccountRepository::forEach(
    std::function<bool(const domain::identity::Account&)> predicate) const {
    if (!conn_) return;

    // forEach has a void contract; a query error simply yields no iterations.
    (void)conn_->query(
        "SELECT id, name, locale, password_hash, locked, "
        "must_change_password, command_groups, created_at, updated_at "
        "FROM accounts",
        [&predicate](const Row& row) {
            try {
                auto account = account_from_row(row);
                return predicate(account);
            } catch (...) {
                return false;
            }
        });
}

std::size_t MySQLAccountRepository::size() const noexcept {
    if (!conn_) return 0;
    std::size_t count = 0;
    // On query failure count stays 0, matching the noexcept/size() contract.
    (void)conn_->query("SELECT COUNT(*) FROM accounts",
                 [&count](const Row& row) {
                     count = static_cast<std::size_t>(row.get_int(0));
                     return false;
                 });
    return count;
}

}  // namespace pvpgn::infra::mysql

#endif  // PVPGN_V3_WITH_MYSQL
