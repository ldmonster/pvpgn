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
    password_hash BLOB NOT NULL,
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

-- IP address bans (can be ranges)
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

-- Clans
CREATE TABLE IF NOT EXISTS clans (
    id INTEGER PRIMARY KEY,
    tag TEXT NOT NULL,
    name TEXT NOT NULL,
    founder_id INTEGER NOT NULL,
    motd TEXT NOT NULL DEFAULT '',
    created_at INTEGER NOT NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS clans_tag ON clans(tag COLLATE NOCASE);

-- Clan membership
CREATE TABLE IF NOT EXISTS clan_members (
    clan_id INTEGER NOT NULL REFERENCES clans(id) ON DELETE CASCADE,
    account_id INTEGER NOT NULL,
    rank INTEGER NOT NULL DEFAULT 0,
    joined_at INTEGER NOT NULL,
    PRIMARY KEY (clan_id, account_id)
);

-- Friend lists (unidirectional)
CREATE TABLE IF NOT EXISTS friend_lists (
    owner_id INTEGER NOT NULL,
    friend_id INTEGER NOT NULL,
    added_at INTEGER NOT NULL,
    PRIMARY KEY (owner_id, friend_id)
);

-- Ladder entries per client tag (STAR, DIAB, D2DV, D2XP, WAR3, W3XP)
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

-- Realms (D2 servers)
CREATE TABLE IF NOT EXISTS realms (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    host TEXT NOT NULL,
    port INTEGER NOT NULL,
    active INTEGER NOT NULL DEFAULT 1
);
CREATE UNIQUE INDEX IF NOT EXISTS realms_name ON realms(name COLLATE NOCASE);
