-- SPDX-License-Identifier: GPL-2.0-or-later
-- Migration: V0001__create_accounts
-- Creates the accounts table for all backends

-- dialect: all
CREATE TABLE accounts (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    locale TEXT NOT NULL DEFAULT 'enUS',
    password_hash TEXT NOT NULL,
    locked INTEGER NOT NULL DEFAULT 0,
    must_change_password INTEGER NOT NULL DEFAULT 0,
    command_groups TEXT NOT NULL DEFAULT '',
    created_at INTEGER NOT NULL DEFAULT 0,
    updated_at INTEGER NOT NULL DEFAULT 0
);

-- dialect: sqlite
CREATE INDEX idx_accounts_name ON accounts(name COLLATE NOCASE);

-- dialect: mysql
CREATE INDEX idx_accounts_name ON accounts(name);
ALTER TABLE accounts CONVERT TO CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

-- dialect: postgres
CREATE INDEX idx_accounts_name ON accounts(name);
