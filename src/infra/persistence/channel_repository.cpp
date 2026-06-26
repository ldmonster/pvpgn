// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/persistence/channel_repository.hpp"

#include <cstdint>
#include <optional>

namespace pvpgn::infra::persistence {

// static
domain::chat::Channel SqlChannelRepository::channel_from_row(const DbRow& row) {
    // Column order: id, name, topic, flags, max_members
    const auto id = domain::ChannelId{
        static_cast<std::uint32_t>(row.get_int(0))};
    const std::string name  = row.get_text(1);
    const std::string topic = row.get_text(2);

    const auto flags_raw   = static_cast<std::uint16_t>(row.get_int(3) & 0xFFFF);
    const auto max_members = static_cast<std::uint32_t>(row.get_int(4));

    domain::chat::ChannelFlags flags;
    for (std::uint8_t bit = 0; bit < 16; ++bit) {
        if (flags_raw & (1u << bit)) {
            flags.set(static_cast<domain::chat::ChannelFlag>(bit));
        }
    }

    domain::chat::ChannelPolicy policy;
    policy.flags       = flags;
    policy.max_members = max_members;

    return domain::chat::Channel::rehydrate(id, name, topic, policy, {}, {});
}

core::Result<domain::chat::Channel>
SqlChannelRepository::find_by_name(const std::string& name) const {
    if (!driver_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "persistence: driver not available"});
    }
    std::optional<domain::chat::Channel> found;
    // Channel names match case-insensitively (original uses strcasecmp). COLLATE
    // NOCASE on the comparison keeps the lookup robust even against rows in a
    // pre-existing DB whose `name` column lacks the column-level collation.
    auto qr = driver_->query_bind(
        "SELECT id, name, topic, flags, max_members FROM channels "
        "WHERE name = ? COLLATE NOCASE",
        {name},
        [&found](const DbRow& row) { found = channel_from_row(row); return false; });
    if (!qr.has_value()) {
        return core::fail(qr.error());
    }
    if (!found) {
        return core::fail(core::Error{
            core::StatusCode::NotFound, "channel: name not found"});
    }
    return *found;
}

core::Result<domain::chat::Channel>
SqlChannelRepository::find_by_id(domain::ChannelId id) const {
    if (!driver_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "persistence: driver not available"});
    }
    std::optional<domain::chat::Channel> found;
    auto qr = driver_->query_bind(
        "SELECT id, name, topic, flags, max_members FROM channels WHERE id = ?",
        {static_cast<std::int64_t>(id.value())},
        [&found](const DbRow& row) { found = channel_from_row(row); return false; });
    if (!qr.has_value()) {
        return core::fail(qr.error());
    }
    if (!found) {
        return core::fail(core::Error{
            core::StatusCode::NotFound, "channel: id not found"});
    }
    return *found;
}

core::Status<> SqlChannelRepository::save(const domain::chat::Channel& channel) {
    if (!driver_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "persistence: driver not available"});
    }
    std::uint16_t flags_raw = 0;
    for (std::uint8_t bit = 0; bit < 16; ++bit) {
        if (channel.policy().flags.has(
                static_cast<domain::chat::ChannelFlag>(bit))) {
            flags_raw |= static_cast<std::uint16_t>(1u << bit);
        }
    }
    // SECURITY: every value is bound via `?` placeholders, never concatenated
    // into the SQL text. `channel.name()` and `channel.topic()` are
    // attacker-controlled free text (channel auto-create on SID_JOINCHANNEL /
    // SetChannelTopic, no charset restriction); building the statement by string
    // concatenation was a stacked-query SQL-injection sink via sqlite3_exec.
    return driver_->query_bind(
        "INSERT OR REPLACE INTO channels (id, name, topic, flags, max_members) "
        "VALUES (?, ?, ?, ?, ?)",
        {static_cast<std::int64_t>(channel.id().value()),
         channel.name(),
         channel.topic(),
         static_cast<std::int64_t>(flags_raw),
         static_cast<std::int64_t>(channel.policy().max_members)},
        [](const DbRow&) { return false; });
}

core::Status<> SqlChannelRepository::remove(domain::ChannelId id) {
    if (!driver_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "persistence: driver not available"});
    }
    return driver_->query_bind(
        "DELETE FROM channels WHERE id = ?",
        {static_cast<std::int64_t>(id.value())},
        [](const DbRow&) { return false; });
}

void SqlChannelRepository::forEach(
    std::function<bool(const domain::chat::Channel&)> predicate) const {
    if (!driver_) return;
    (void)driver_->query(
        "SELECT id, name, topic, flags, max_members FROM channels ORDER BY id ASC",
        [&predicate](const DbRow& row) {
            auto channel = channel_from_row(row);
            return predicate(channel);
        });
}

std::size_t SqlChannelRepository::size() const noexcept {
    if (!driver_) return 0;
    std::size_t count = 0;
    (void)driver_->query(
        "SELECT COUNT(*) FROM channels",
        [&count](const DbRow& row) {
            count = static_cast<std::size_t>(row.get_int(0));
            return false;
        });
    return count;
}

}  // namespace pvpgn::infra::persistence
