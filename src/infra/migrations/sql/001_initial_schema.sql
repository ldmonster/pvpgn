-- SPDX-License-Identifier: GPL-2.0-or-later
-- Initial schema for pvpgn v3
-- Creates base tables for accounts, clans, ladder, and bans

CREATE TABLE IF NOT EXISTS _schema_migrations (
    version INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    applied_at INTEGER NOT NULL,
    checksum TEXT NOT NULL
);

-- Accounts: core identity table
CREATE TABLE IF NOT EXISTS accounts (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL COLLATE NOCASE,
    locale TEXT NOT NULL DEFAULT 'enUS',
    -- 40-char lowercase hex of the 20-byte BNHash; stored/read as TEXT.
    password_hash TEXT NOT NULL,
    locked INTEGER NOT NULL DEFAULT 0,
    must_change_password INTEGER NOT NULL DEFAULT 0,
    command_groups TEXT NOT NULL DEFAULT '',
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS accounts_name ON accounts(name COLLATE NOCASE);

-- Extended attributes for accounts (key-value pairs)
CREATE TABLE IF NOT EXISTS account_attributes (
    account_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    key TEXT NOT NULL,
    value TEXT NOT NULL,
    PRIMARY KEY (account_id, key)
);

-- Account bans
CREATE TABLE IF NOT EXISTS account_bans (
    account_id INTEGER PRIMARY KEY REFERENCES accounts(id) ON DELETE CASCADE,
    banned_by INTEGER NOT NULL,
    reason TEXT NOT NULL DEFAULT '',
    banned_at INTEGER NOT NULL,
    expires_at INTEGER
);

-- Exact-host IP bans (columns mirror SqlIpBanRepository)
CREATE TABLE IF NOT EXISTS ip_bans (
    ip TEXT PRIMARY KEY,
    reason TEXT NOT NULL DEFAULT '',
    issuer INTEGER NOT NULL,
    issued_at INTEGER NOT NULL,
    expires_at INTEGER
);
CREATE INDEX IF NOT EXISTS ip_bans_address ON ip_bans(ip);

-- CIDR range bans (second table the IP-ban repo requires)
CREATE TABLE IF NOT EXISTS ip_ban_ranges (
    network TEXT NOT NULL,
    prefix_bits INTEGER NOT NULL,
    reason TEXT NOT NULL DEFAULT '',
    issuer INTEGER NOT NULL,
    issued_at INTEGER NOT NULL,
    expires_at INTEGER,
    PRIMARY KEY (network, prefix_bits)
);

-- Clans (columns mirror SqlClanRepository)
CREATE TABLE IF NOT EXISTS clans (
    id INTEGER PRIMARY KEY,
    tag TEXT NOT NULL,
    name TEXT NOT NULL,
    client_tag TEXT NOT NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS clans_tag ON clans(tag COLLATE NOCASE);

-- Clan membership (ordered by position)
CREATE TABLE IF NOT EXISTS clan_members (
    clan_id INTEGER NOT NULL REFERENCES clans(id) ON DELETE CASCADE,
    account_id INTEGER NOT NULL,
    rank INTEGER NOT NULL DEFAULT 0,
    position INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (clan_id, account_id)
);

-- Friend lists (unidirectional, ordered by position) — table `friends`
CREATE TABLE IF NOT EXISTS friends (
    owner_id INTEGER NOT NULL,
    friend_id INTEGER NOT NULL,
    position INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (owner_id, friend_id)
);

-- Ladder (account-keyed; client_tag defaults to '' to satisfy the PK)
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

-- Realms (D2 servers); host/port carry defaults (repo persists only id/name/desc/active)
CREATE TABLE IF NOT EXISTS realms (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    host TEXT NOT NULL DEFAULT '',
    port INTEGER NOT NULL DEFAULT 0,
    active INTEGER NOT NULL DEFAULT 1
);
CREATE UNIQUE INDEX IF NOT EXISTS realms_name ON realms(name COLLATE NOCASE);
