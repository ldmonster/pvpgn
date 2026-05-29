-- SPDX-License-Identifier: GPL-2.0-or-later
-- Migration 002: channels table
-- Stores persistent channel definitions (permanent/system channels).

CREATE TABLE IF NOT EXISTS channels (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE,
    topic TEXT NOT NULL DEFAULT '',
    flags INTEGER NOT NULL DEFAULT 0,
    max_members INTEGER NOT NULL DEFAULT 0,
    created_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))
);
