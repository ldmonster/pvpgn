// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/persistence/account_repository.hpp"

#include <sstream>

#include "domain/shared/bn_hash.hpp"
#include "domain/shared/locale.hpp"

namespace pvpgn::infra::persistence {

namespace {

/// Decode a 40-character hex string into a BNHash. Zeroed on any parse error.
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

domain::identity::Account SqlAccountRepository::account_from_row(
    const DbRow& row) {
    // Column order: id, name, locale, password_hash, locked,
    //               must_change_password, command_groups, created_at, updated_at
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
        std::nullopt,  // active_ban — fetched separately if needed
        locked, must_change_password);
}

core::Result<domain::identity::Account> SqlAccountRepository::find_by_id(
    domain::AccountId id) const {
    if (!driver_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "persistence: driver not available"});
    }

    std::optional<domain::identity::Account> found;
    auto q = driver_->query_bind(
        "SELECT id, name, locale, password_hash, locked, must_change_password, "
        "command_groups, created_at, updated_at FROM accounts WHERE id = ?",
        {static_cast<std::int64_t>(id.value())},
        [&found](const DbRow& row) {
            try {
                found = account_from_row(row);
            } catch (...) {}
            return false;  // stop after the first row
        });
    if (!q.has_value()) {
        return core::fail(q.error());
    }
    if (!found) {
        return core::fail(core::Error{
            core::StatusCode::NotFound, "account: id not found"});
    }
    return *found;
}

core::Result<domain::identity::Account> SqlAccountRepository::find_by_name(
    const domain::UserName& name) const {
    if (!driver_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "persistence: driver not available"});
    }

    std::optional<domain::identity::Account> found;
    auto q = driver_->query_bind(
        "SELECT id, name, locale, password_hash, locked, must_change_password, "
        "command_groups, created_at, updated_at FROM accounts WHERE name = ? "
        "COLLATE NOCASE",
        {std::string{name.canonical()}},
        [&found](const DbRow& row) {
            try {
                found = account_from_row(row);
            } catch (...) {}
            return false;
        });
    if (!q.has_value()) {
        return core::fail(q.error());
    }
    if (!found) {
        return core::fail(core::Error{
            core::StatusCode::NotFound, "account: name not found"});
    }
    return *found;
}

core::Status<> SqlAccountRepository::save(
    const domain::identity::Account& account) {
    if (!driver_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "persistence: driver not available"});
    }

    std::ostringstream groups_oss;
    bool first_group = true;
    for (std::uint8_t g = 1; g <= domain::identity::CommandGroupMask::kBits; ++g) {
        if (account.command_groups().has(g)) {
            if (!first_group) groups_oss << ",";
            groups_oss << static_cast<int>(g);
            first_group = false;
        }
    }

    // Upsert. "INSERT OR REPLACE" is SQLite/MySQL-compatible; the dialect layer
    // rewrites it to "ON CONFLICT ... DO UPDATE" for PostgreSQL (tracked).
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
        << "0, 0);";

    return driver_->exec(sql.str());
}

core::Status<> SqlAccountRepository::remove(domain::AccountId id) {
    if (!driver_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "persistence: driver not available"});
    }
    std::ostringstream sql;
    sql << "DELETE FROM accounts WHERE id = " << id.value() << ";";
    return driver_->exec(sql.str());
}

void SqlAccountRepository::forEach(
    std::function<bool(const domain::identity::Account&)> predicate) const {
    if (!driver_) {
        return;
    }
    (void)driver_->query(
        "SELECT id, name, locale, password_hash, locked, must_change_password, "
        "command_groups, created_at, updated_at FROM accounts",
        [&predicate](const DbRow& row) {
            try {
                auto account = account_from_row(row);
                return predicate(account);
            } catch (...) {
                return false;
            }
        });
}

std::size_t SqlAccountRepository::size() const noexcept {
    if (!driver_) {
        return 0;
    }
    std::size_t count = 0;
    (void)driver_->query(
        "SELECT COUNT(*) FROM accounts",
        [&count](const DbRow& row) {
            count = static_cast<std::size_t>(row.get_int(0));
            return false;
        });
    return count;
}

}  // namespace pvpgn::infra::persistence
