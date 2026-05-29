// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/account_repository.hpp"

#include <sstream>

#include "domain/identity/account.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"

namespace pvpgn::infra::sqlite {

SQLiteAccountRepository::SQLiteAccountRepository(
    std::shared_ptr<SQLiteConnection> conn)
    : conn_(std::move(conn)) {}

namespace {

/// Decode a 40-character hex string into a BNHash.
/// Returns a zeroed BNHash on any parse error.
domain::BNHash bn_hash_from_hex(std::string_view hex) {
    if (hex.size() != 40) {
        return domain::BNHash{};
    }
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

}  // namespace

domain::identity::Account SQLiteAccountRepository::account_from_row(
    const Row& row) {
    // Column order: id, name, locale, password_hash, locked,
    //               must_change_password, command_groups, created_at, updated_at
    const auto id = domain::AccountId{
        static_cast<std::uint32_t>(row.get_int(0))};

    // Parse UserName — fall back to a placeholder on invalid data
    auto name_result = domain::UserName::parse(row.get_text(1));
    auto name = name_result.has_value()
                    ? std::move(name_result.value())
                    : domain::UserName::parse("unknown").value();

    const domain::Locale locale =
        domain::Locale::parse_or_default(row.get_text(2));

    const domain::BNHash password = bn_hash_from_hex(row.get_text(3));

    const bool locked              = row.get_int(4) != 0;
    const bool must_change_password = row.get_int(5) != 0;

    // Parse command groups (comma-separated list of group numbers)
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
        id,
        std::move(name),
        password,
        locale,
        groups,
        std::nullopt,          // active_ban — fetched separately if needed
        locked,
        must_change_password
    );
}

core::Result<domain::identity::Account> SQLiteAccountRepository::find_by_id(
    domain::AccountId id) const {
    if (!conn_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not available"});
    }

    domain::identity::Account* result = nullptr;

    auto query_result = conn_->query_bind(
        "SELECT id, name, locale, password_hash, locked, must_change_password, "
        "command_groups, created_at, updated_at FROM accounts WHERE id = ?",
        {static_cast<std::int64_t>(id.value())},
        [&result](const Row& row) {
            try {
                result = new domain::identity::Account{account_from_row(row)};
                return false;  // Stop iteration
            } catch (...) {
                return false;
            }
        });

    if (!query_result.has_value()) {
        return core::fail(query_result.error());
    }

    if (!result) {
        return core::fail(core::Error{
            core::StatusCode::NotFound, "account: id not found"});
    }

    auto account = *result;
    delete result;
    return account;
}

core::Result<domain::identity::Account> SQLiteAccountRepository::find_by_name(
    const domain::UserName& name) const {
    if (!conn_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not available"});
    }

    domain::identity::Account* result = nullptr;

    auto query_result = conn_->query_bind(
        "SELECT id, name, locale, password_hash, locked, must_change_password, "
        "command_groups, created_at, updated_at FROM accounts WHERE name = ? "
        "COLLATE NOCASE",
        {std::string{name.canonical()}},
        [&result](const Row& row) {
            try {
                result = new domain::identity::Account{account_from_row(row)};
                return false;  // Stop iteration
            } catch (...) {
                return false;
            }
        });

    if (!query_result.has_value()) {
        return core::fail(query_result.error());
    }

    if (!result) {
        return core::fail(core::Error{
            core::StatusCode::NotFound, "account: name not found"});
    }

    auto account = *result;
    delete result;
    return account;
}

core::Status<> SQLiteAccountRepository::save(
    const domain::identity::Account& account) {
    if (!conn_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not available"});
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

    // INSERT OR REPLACE into accounts table
    // Use canonical (lower-case) name for the indexed column; display name
    // is stored in the name column for round-trip fidelity.
    std::ostringstream sql;
    sql << "INSERT OR REPLACE INTO accounts "
        << "(id, name, locale, password_hash, locked, must_change_password, "
        << "command_groups, created_at, updated_at) "
        << "VALUES (" << account.id().value() << ", '"
        << account.name().display() << "', '"
        << account.locale().text() << "', '"
        << bn_hash_to_hex(account.password_hash1()) << "', "
        << (account.is_locked() ? 1 : 0) << ", "
        << (account.must_change_password() ? 1 : 0) << ", '"
        << groups_oss.str() << "', "
        << "0, 0);";  // created_at / updated_at: not tracked in domain model

    return conn_->exec(sql.str());
}

core::Status<> SQLiteAccountRepository::remove(domain::AccountId id) {
    if (!conn_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not available"});
    }

    std::ostringstream sql;
    sql << "DELETE FROM accounts WHERE id = " << id.value() << ";";

    auto result = conn_->exec(sql.str());

    // Check if anything was deleted by querying count
    if (result.has_value()) {
        // Verify account existed - do a quick check
        // For now, assume deletion succeeded
        return core::ok();
    }

    return result;
}

void SQLiteAccountRepository::forEach(
    std::function<bool(const domain::identity::Account&)> predicate) const {
    if (!conn_) {
        return;
    }

    conn_->query(
        "SELECT id, name, locale, password_hash, locked, must_change_password, "
        "command_groups, created_at, updated_at FROM accounts",
        [&predicate](const Row& row) {
            try {
                auto account = account_from_row(row);
                return predicate(account);
            } catch (...) {
                return false;
            }
        });
}

std::size_t SQLiteAccountRepository::size() const noexcept {
    if (!conn_) {
        return 0;
    }

    std::size_t count = 0;
    conn_->query(
        "SELECT COUNT(*) FROM accounts",
        [&count](const Row& row) {
            count = static_cast<std::size_t>(row.get_int(0));
            return false;  // Stop after first row
        });

    return count;
}

}  // namespace pvpgn::infra::sqlite
