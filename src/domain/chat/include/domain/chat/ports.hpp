// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
//
// domain/chat/ports.hpp — Abstract ports (interfaces) for the chat bounded context.
// Implementations live in src/infra/<tech>/ and src/integration/<binding>/.
// Plan 05: Ports Consolidation (migrated from application/ports/)

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/chat/channel.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain::chat {

// ---------------------------------------------------------------------------
// IChannelRepository
// ---------------------------------------------------------------------------

// Segregated into reader + writer (ADR 0012 / ISP): read-only consumers such as
// ListChannels depend on IChannelReader only. IChannelRepository = reader +
// writer remains for callers/implementers that need both.

/// Read side of the channel repository.
class IChannelReader {
public:
    virtual ~IChannelReader() = default;

    [[nodiscard]] virtual core::Result<Channel>
    find_by_id(domain::ChannelId id) const = 0;

    [[nodiscard]] virtual core::Result<Channel>
    find_by_name(const std::string& name) const = 0;

    virtual void forEach(
        std::function<bool(const Channel&)> predicate) const = 0;

    [[nodiscard]] virtual std::size_t size() const noexcept = 0;

protected:
    IChannelReader() = default;
};

/// Write side of the channel repository.
class IChannelWriter {
public:
    virtual ~IChannelWriter() = default;

    virtual core::Status<> save(const Channel& channel) = 0;
    virtual core::Status<> remove(domain::ChannelId id) = 0;

protected:
    IChannelWriter() = default;
};

/// Full channel repository: read + write. Implementers derive from this and
/// override all six methods exactly as before.
class IChannelRepository : public IChannelReader, public IChannelWriter {
public:
    IChannelRepository(const IChannelRepository&)            = delete;
    IChannelRepository& operator=(const IChannelRepository&) = delete;
    IChannelRepository(IChannelRepository&&)                 = delete;
    IChannelRepository& operator=(IChannelRepository&&)      = delete;
    ~IChannelRepository() override                           = default;

protected:
    IChannelRepository() = default;
};

// ---------------------------------------------------------------------------
// ChannelDefinition struct + IChannelStore
// ---------------------------------------------------------------------------

/// Durable channel record — just the data persisted to disk,
/// without any runtime membership.
struct ChannelDefinition {
    std::string   name;
    std::string   topic;
    std::uint32_t max_users    = 0;
    bool          is_permanent = false;
    bool          is_moderated = false;
};

class IChannelStore {
public:
    virtual ~IChannelStore() = default;

    /// Load every stored channel definition.
    [[nodiscard]] virtual core::Status<std::vector<ChannelDefinition>>
        load_all() const = 0;

    /// Insert or replace a channel definition (keyed by `def.name`).
    virtual core::Status<void> save(ChannelDefinition def) = 0;

    /// Remove the definition with the given name.
    virtual core::Status<void> remove(std::string_view name) = 0;
};

// ---------------------------------------------------------------------------
// IMessageBroadcaster
// ---------------------------------------------------------------------------

class IMessageBroadcaster {
public:
    virtual ~IMessageBroadcaster() = default;

    /// Broadcast `message` to every member of `channel_id`.
    virtual void
        broadcast_to_channel(domain::ChannelId channel_id,
                             std::string_view  message) = 0;

    /// Send an INFO-class message to a single connection.
    virtual void
        send_info(domain::ConnectionId connection_id,
                  std::string_view     message) = 0;

    /// Send an ERROR-class message to a single connection.
    virtual void
        send_error(domain::ConnectionId connection_id,
                   std::string_view     message) = 0;
};

// ---------------------------------------------------------------------------
// IHelpfileSource
// ---------------------------------------------------------------------------

class IHelpfileSource {
public:
    virtual ~IHelpfileSource() = default;

    /// Return the help text for `command_name`, or `std::nullopt` if the
    /// command has no registered help entry.
    [[nodiscard]] virtual std::optional<std::string>
        lookup(std::string_view command_name) const = 0;

    /// Return all commands that have registered help entries.
    [[nodiscard]] virtual std::vector<std::string>
        all_commands() const = 0;
};

} // namespace pvpgn::domain::chat
