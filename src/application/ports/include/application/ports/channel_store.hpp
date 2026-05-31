// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file channel_store.hpp
/// Port for durable persistence of channel definitions
/// (name, topic, flags, max-members) — durable side of
/// `IChannelRepository`.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::application::ports {

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

} // namespace pvpgn::application::ports
