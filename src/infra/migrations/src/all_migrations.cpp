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
    -- 40-char lowercase hex of the 20-byte BNHash (see SqlAccountRepository).
    -- Declared TEXT to match what the writer binds and the reader (get_text)
    -- expects; a BLOB-affinity column would misrepresent the stored ASCII hex on
    -- stricter backends (Postgres BYTEA / MySQL BLOB).
    password_hash TEXT NOT NULL,
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

-- Exact-host IP bans. Column names mirror SqlIpBanRepository's INSERT/SELECT
-- (ip, reason, issuer, issued_at, expires_at); `ip` is the primary key the repo
-- upserts/deletes on.
CREATE TABLE IF NOT EXISTS ip_bans (
    ip TEXT PRIMARY KEY,
    reason TEXT NOT NULL DEFAULT '',
    issuer INTEGER NOT NULL,
    issued_at INTEGER NOT NULL,
    expires_at INTEGER
);
CREATE INDEX IF NOT EXISTS ip_bans_address ON ip_bans(ip);

-- CIDR range bans — the second table SqlIpBanRepository's add_range_ban /
-- remove_range_ban / is_banned(CIDR path) require. Keyed by (network, bits).
CREATE TABLE IF NOT EXISTS ip_ban_ranges (
    network TEXT NOT NULL,
    prefix_bits INTEGER NOT NULL,
    reason TEXT NOT NULL DEFAULT '',
    issuer INTEGER NOT NULL,
    issued_at INTEGER NOT NULL,
    expires_at INTEGER,
    PRIMARY KEY (network, prefix_bits)
);

-- Clans: parent row (id, tag, name, client_tag) + ordered membership. Columns
-- mirror SqlClanRepository's INSERT/SELECT exactly.
CREATE TABLE IF NOT EXISTS clans (
    id INTEGER PRIMARY KEY,
    tag TEXT NOT NULL,
    name TEXT NOT NULL,
    client_tag TEXT NOT NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS clans_tag ON clans(tag COLLATE NOCASE);

CREATE TABLE IF NOT EXISTS clan_members (
    clan_id INTEGER NOT NULL REFERENCES clans(id) ON DELETE CASCADE,
    account_id INTEGER NOT NULL,
    rank INTEGER NOT NULL DEFAULT 0,
    position INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (clan_id, account_id)
);

-- Friend lists (unidirectional, ordered). SqlFriendListRepository reads/writes
-- table `friends` with a 0-based `position` for ordering.
CREATE TABLE IF NOT EXISTS friends (
    owner_id INTEGER NOT NULL,
    friend_id INTEGER NOT NULL,
    position INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (owner_id, friend_id)
);

-- Ladder. SqlLadderRepository keys by account_id alone and writes
-- (account_id, rating, wins, losses, disconnects). `client_tag` defaults to ''
-- so the repo's account-keyed INSERT satisfies the composite primary key.
CREATE TABLE IF NOT EXISTS ladder (
    account_id INTEGER NOT NULL,
    client_tag TEXT NOT NULL DEFAULT '',
    rating INTEGER NOT NULL DEFAULT 1000,
    wins INTEGER NOT NULL DEFAULT 0,
    losses INTEGER NOT NULL DEFAULT 0,
    disconnects INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (account_id, client_tag)
);
CREATE INDEX IF NOT EXISTS ladder_rating ON ladder(rating DESC);

-- Realms (D2 servers). host/port carry defaults: SqlRealmRepository persists
-- only (id, name, description, active), so the network address columns must not
-- be NOT NULL-without-default or the repo's INSERT would fail the constraint.
CREATE TABLE IF NOT EXISTS realms (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    host TEXT NOT NULL DEFAULT '',
    port INTEGER NOT NULL DEFAULT 0,
    active INTEGER NOT NULL DEFAULT 1
);
CREATE UNIQUE INDEX IF NOT EXISTS realms_name ON realms(name COLLATE NOCASE);
)";

// Down migration (empty for rollback of initial schema)
constexpr std::string_view migration_001_down = "";

// Embedded migration SQL (002_channels.sql)
// Channel names match case-insensitively (original uses strcasecmp); COLLATE
// NOCASE makes both lookups and the UNIQUE constraint case-folding so "War3"
// and "war3" resolve to the same row.
constexpr std::string_view migration_002_up = R"(
CREATE TABLE IF NOT EXISTS channels (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE COLLATE NOCASE,
    topic TEXT NOT NULL DEFAULT '',
    flags INTEGER NOT NULL DEFAULT 0,
    max_members INTEGER NOT NULL DEFAULT 0,
    created_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))
);
)";

constexpr std::string_view migration_002_down = "";

// Static migration registry (compile-time constant)
constexpr std::array<Migration, 2> all_migrations_array{{
    Migration{
        .version = 1,
        .name = "001_initial_schema",
        .up_sql = migration_001_up,
        .down_sql = migration_001_down,
    },
    Migration{
        .version = 2,
        .name = "002_channels",
        .up_sql = migration_002_up,
        .down_sql = migration_002_down,
    },
}};

std::span<const Migration> get_all_migrations() {
    return all_migrations_array;
}

}  // namespace pvpgn::infra::migrations
