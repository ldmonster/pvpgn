// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file sqlite_channel_repository.hpp
/// SQLite-backed channel repository.
///
/// Persists permanent/system channel definitions to the `channels` table
/// (created by migration 002_channels). Runtime session-scoped membership
/// is NOT stored here — only the durable channel record (name, topic,
/// flags, max_members) is persisted.

#include <memory>

#include "domain/chat/ports.hpp"
#include "infra/sqlite/connection.hpp"

namespace pvpgn::infra::sqlite {

class SqliteChannelRepository final
    : public application::ports::IChannelRepository {
public:
    explicit SqliteChannelRepository(std::shared_ptr<SQLiteConnection> conn);

    /// Find a channel by name (case-sensitive lookup against stored name).
    core::Result<domain::chat::Channel>
    find_by_name(const std::string& name) const override;

    /// Find a channel by its numeric ID.
    core::Result<domain::chat::Channel>
    find_by_id(domain::ChannelId id) const override;

    /// Insert or replace a channel record.
    core::Status<> save(const domain::chat::Channel& channel) override;

    /// Delete a channel record by ID.
    core::Status<> remove(domain::ChannelId id) override;

    /// Iterate over all stored channel records.
    void forEach(std::function<bool(const domain::chat::Channel&)> predicate)
        const override;

    /// Return the number of stored channel records.
    std::size_t size() const noexcept override;

private:
    std::shared_ptr<SQLiteConnection> conn_;

    /// Reconstruct a Channel aggregate from a query row.
    /// Column order: id, name, topic, flags, max_members
    static domain::chat::Channel channel_from_row(const Row& row);
};

}  // namespace pvpgn::infra::sqlite
