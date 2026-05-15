// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/migrations/all_migrations.hpp"

#include <array>

namespace pvpgn::infra::migrations {

// Embedded migration SQL (001_initial_schema.sql)
constexpr std::string_view migration_001_up = R"(
CREATE TABLE IF NOT EXISTS _schema_migrations (
    version INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    applied_at INTEGER NOT NULL,
    checksum TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS accounts (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL COLLATE NOCASE,
    locale TEXT NOT NULL DEFAULT 'enUS',
    password_hash BLOB NOT NULL,
    locked INTEGER NOT NULL DEFAULT 0,
    must_change_password INTEGER NOT NULL DEFAULT 0,
    command_groups TEXT NOT NULL DEFAULT '',
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS accounts_name ON accounts(name COLLATE NOCASE);

CREATE TABLE IF NOT EXISTS account_attributes (
    account_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    key TEXT NOT NULL,
    value TEXT NOT NULL,
    PRIMARY KEY (account_id, key)
);

CREATE TABLE IF NOT EXISTS account_bans (
    account_id INTEGER PRIMARY KEY REFERENCES accounts(id) ON DELETE CASCADE,
    banned_by INTEGER NOT NULL,
    reason TEXT NOT NULL DEFAULT '',
    banned_at INTEGER NOT NULL,
    expires_at INTEGER
);

CREATE TABLE IF NOT EXISTS ip_bans (
    id INTEGER PRIMARY KEY,
    ip_address TEXT NOT NULL,
    is_range INTEGER NOT NULL DEFAULT 0,
    banner_account_id INTEGER,
    reason TEXT NOT NULL DEFAULT '',
    banned_at INTEGER NOT NULL,
    expires_at INTEGER
);
CREATE INDEX IF NOT EXISTS ip_bans_address ON ip_bans(ip_address);

CREATE TABLE IF NOT EXISTS clans (
    id INTEGER PRIMARY KEY,
    tag TEXT NOT NULL,
    name TEXT NOT NULL,
    founder_id INTEGER NOT NULL,
    motd TEXT NOT NULL DEFAULT '',
    created_at INTEGER NOT NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS clans_tag ON clans(tag COLLATE NOCASE);

CREATE TABLE IF NOT EXISTS clan_members (
    clan_id INTEGER NOT NULL REFERENCES clans(id) ON DELETE CASCADE,
    account_id INTEGER NOT NULL,
    rank INTEGER NOT NULL DEFAULT 0,
    joined_at INTEGER NOT NULL,
    PRIMARY KEY (clan_id, account_id)
);

CREATE TABLE IF NOT EXISTS friend_lists (
    owner_id INTEGER NOT NULL,
    friend_id INTEGER NOT NULL,
    added_at INTEGER NOT NULL,
    PRIMARY KEY (owner_id, friend_id)
);

CREATE TABLE IF NOT EXISTS ladder_entries (
    account_id INTEGER NOT NULL,
    client_tag TEXT NOT NULL,
    wins INTEGER NOT NULL DEFAULT 0,
    losses INTEGER NOT NULL DEFAULT 0,
    disconnects INTEGER NOT NULL DEFAULT 0,
    rating INTEGER NOT NULL DEFAULT 1000,
    rank INTEGER NOT NULL DEFAULT 0,
    updated_at INTEGER NOT NULL,
    PRIMARY KEY (account_id, client_tag)
);
CREATE INDEX IF NOT EXISTS ladder_rating ON ladder_entries(client_tag, rating DESC);

CREATE TABLE IF NOT EXISTS realms (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    host TEXT NOT NULL,
    port INTEGER NOT NULL,
    active INTEGER NOT NULL DEFAULT 1
);
CREATE UNIQUE INDEX IF NOT EXISTS realms_name ON realms(name COLLATE NOCASE);
)";

// Down migration (empty for rollback of initial schema)
constexpr std::string_view migration_001_down = "";

// Static migration registry (compile-time constant)
constexpr std::array<Migration, 1> all_migrations_array{{
    Migration{
        .version = 1,
        .name = "001_initial_schema",
        .up_sql = migration_001_up,
        .down_sql = migration_001_down,
    },
}};

std::span<const Migration> get_all_migrations() {
    return all_migrations_array;
}

}  // namespace pvpgn::infra::migrations
