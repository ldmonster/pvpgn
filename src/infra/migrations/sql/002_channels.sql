-- SPDX-License-Identifier: GPL-2.0-or-later
-- Migration 002: channels table
-- Stores persistent channel definitions (permanent/system channels).

-- Channel names match case-insensitively, mirroring the original server's
-- strcasecmp-based lookup (channellist_find_channel_by_name). COLLATE NOCASE
-- makes both equality lookups and the UNIQUE constraint case-folding, so
-- "War3" and "war3" resolve to the same row instead of creating a duplicate.
CREATE TABLE IF NOT EXISTS channels (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE COLLATE NOCASE,
    topic TEXT NOT NULL DEFAULT '',
    flags INTEGER NOT NULL DEFAULT 0,
    max_members INTEGER NOT NULL DEFAULT 0,
    created_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))
);
