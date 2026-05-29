// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/sqlite_channel_repository.hpp"

#include <cstdint>
#include <sstream>

#include "domain/chat/channel.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::infra::sqlite {

SqliteChannelRepository::SqliteChannelRepository(
    std::shared_ptr<SQLiteConnection> conn)
    : conn_(std::move(conn)) {}

// static
domain::chat::Channel SqliteChannelRepository::channel_from_row(
    const Row& row) {
    // Column order: id, name, topic, flags, max_members
    const auto id = domain::ChannelId{
        static_cast<std::uint32_t>(row.get_int(0))};
    const std::string name  = row.get_text(1);
    const std::string topic = row.get_text(2);

    const auto flags_raw =
        static_cast<std::uint8_t>(row.get_int(3) & 0xFF);
    const auto max_members =
        static_cast<std::uint32_t>(row.get_int(4));

    // Reconstruct ChannelFlags from the stored bitmask.
    domain::chat::ChannelFlags flags;
    for (std::uint8_t bit = 0; bit < 8; ++bit) {
        if (flags_raw & (1u << bit)) {
            flags.set(static_cast<domain::chat::ChannelFlag>(bit));
        }
    }

    domain::chat::ChannelPolicy policy;
    policy.flags       = flags;
    policy.max_members = max_members;

    // Rehydrate with no members or banlist — those are session-scoped.
    return domain::chat::Channel::rehydrate(
        id, name, topic, policy, {}, {});
}

core::Result<domain::chat::Channel>
SqliteChannelRepository::find_by_name(const std::string& name) const {
    if (!conn_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not available"});
    }

    domain::chat::Channel* result = nullptr;

    auto qr = conn_->query_bind(
        "SELECT id, name, topic, flags, max_members "
        "FROM channels WHERE name = ?",
        {name},
        [&result](const Row& row) {
            result = new domain::chat::Channel{channel_from_row(row)};
            return false;
        });

    if (!qr.has_value()) {
        return core::fail(qr.error());
    }
    if (!result) {
        return core::fail(core::Error{
            core::StatusCode::NotFound, "channel: name not found"});
    }

    auto channel = *result;
    delete result;
    return channel;
}

core::Result<domain::chat::Channel>
SqliteChannelRepository::find_by_id(domain::ChannelId id) const {
    if (!conn_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not available"});
    }

    domain::chat::Channel* result = nullptr;

    auto qr = conn_->query_bind(
        "SELECT id, name, topic, flags, max_members "
        "FROM channels WHERE id = ?",
        {static_cast<std::int64_t>(id.value())},
        [&result](const Row& row) {
            result = new domain::chat::Channel{channel_from_row(row)};
            return false;
        });

    if (!qr.has_value()) {
        return core::fail(qr.error());
    }
    if (!result) {
        return core::fail(core::Error{
            core::StatusCode::NotFound, "channel: id not found"});
    }

    auto channel = *result;
    delete result;
    return channel;
}

core::Status<> SqliteChannelRepository::save(
    const domain::chat::Channel& channel) {
    if (!conn_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not available"});
    }

    // Encode ChannelFlags back to a bitmask integer.
    std::uint8_t flags_raw = 0;
    for (std::uint8_t bit = 0; bit < 8; ++bit) {
        if (channel.policy().flags.has(
                static_cast<domain::chat::ChannelFlag>(bit))) {
            flags_raw |= static_cast<std::uint8_t>(1u << bit);
        }
    }

    // Use INSERT OR REPLACE to handle both insert and update.
    std::ostringstream sql;
    sql << "INSERT OR REPLACE INTO channels (id, name, topic, flags, max_members) "
        << "VALUES ("
        << static_cast<std::int64_t>(channel.id().value()) << ", '"
        << channel.name() << "', '"
        << channel.topic() << "', "
        << static_cast<int>(flags_raw) << ", "
        << channel.policy().max_members << ");";

    auto result = conn_->exec(sql.str());
    if (!result.has_value()) {
        return core::fail(result.error());
    }
    return core::ok();
}

core::Status<> SqliteChannelRepository::remove(domain::ChannelId id) {
    if (!conn_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not available"});
    }

    std::ostringstream sql;
    sql << "DELETE FROM channels WHERE id = "
        << static_cast<std::int64_t>(id.value()) << ";";

    auto result = conn_->exec(sql.str());
    if (!result.has_value()) {
        return core::fail(result.error());
    }
    return core::ok();
}

void SqliteChannelRepository::forEach(
    std::function<bool(const domain::chat::Channel&)> predicate) const {
    if (!conn_) return;

    conn_->query(
        "SELECT id, name, topic, flags, max_members FROM channels "
        "ORDER BY id ASC",
        [&predicate](const Row& row) {
            auto channel = channel_from_row(row);
            return predicate(channel);
        });
}

std::size_t SqliteChannelRepository::size() const noexcept {
    if (!conn_) return 0;

    std::size_t count = 0;
    conn_->query(
        "SELECT COUNT(*) FROM channels",
        [&count](const Row& row) {
            count = static_cast<std::size_t>(row.get_int(0));
            return false;
        });
    return count;
}

}  // namespace pvpgn::infra::sqlite
