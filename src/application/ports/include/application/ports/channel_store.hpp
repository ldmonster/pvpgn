// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file channel_store.hpp
/// Persistent channel definition store port.
///
/// Distinct from IChannelRepository (runtime in-memory registry of active
/// channels): IChannelStore manages the durable set of channel definitions
/// loaded from config/DB at startup and written back when an admin creates
/// or removes a permanent channel.

#include "core/result.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pvpgn::application::ports {

/// Persistent definition of a channel (config/DB record).
struct ChannelDefinition {
    std::string   name;
    std::string   topic;
    std::uint32_t max_users{0};
    bool          is_permanent{false};  ///< survives when empty
    bool          is_moderated{false};
};

/// Port: persistent channel definition store.
class IChannelStore {
public:
    virtual ~IChannelStore() = default;

    /// Load all persistent channel definitions (called at startup).
    virtual core::Status<std::vector<ChannelDefinition>> load_all() const = 0;

    /// Persist a channel definition (admin creates permanent channel).
    virtual core::Status<void> save(ChannelDefinition def) = 0;

    /// Remove a persistent channel definition by name.
    virtual core::Status<void> remove(std::string_view name) = 0;
};

} // namespace pvpgn::application::ports
