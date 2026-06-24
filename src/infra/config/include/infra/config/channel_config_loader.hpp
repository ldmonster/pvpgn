// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file channel_config_loader.hpp
/// Loads the list of permanent channels from the legacy `channel.conf` file
/// (or returns hardcoded defaults when no file is available).
///
/// The legacy format is a space-delimited columnar file:
///   "special name"  "short name"  cltag  bots  ops  log  ctry  realm  max  mod
///
/// Only the special name (column 0) and max_users (column 8) are consumed by
/// this loader; the rest are ignored for now.  Lines starting with `#` and
/// blank lines are skipped.
///
/// Usage
/// -----
///   auto entries = ChannelConfigLoader::load("/etc/pvpgn/channel.conf");
///   // or, when no config file is available:
///   auto entries = ChannelConfigLoader::defaults();

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace pvpgn::infra::config {

/// A single channel entry parsed from the config file (or hardcoded default).
struct ChannelConfigEntry {
    /// Display name of the channel (the "special name" column, or short name
    /// if special name is NONE).
    std::string name;

    /// Optional topic string (empty if not specified).
    std::string topic;

    /// Channel flags bitmask.
    ///   bit 0 = Permanent
    ///   bit 6 = AllowBots
    /// Default: Permanent | AllowBots (0x41).
    std::uint32_t flags{0x41};

    /// Maximum number of members (0 = unlimited).
    std::uint32_t max_members{0};

    /// Whether the channel is permanent (survives when empty).
    bool permanent{true};
};

/// Loads permanent channel definitions from the legacy `channel.conf` format.
class ChannelConfigLoader {
public:
    /// Parse the legacy channel.conf file at @p config_path.
    ///
    /// Lines starting with `#` and blank lines are skipped.
    /// Each data line has the columnar format:
    ///   "special name"  "short name"  cltag  bots  ops  log  ctry  realm  max  mod
    ///
    /// If @p config_path does not exist or cannot be opened, returns an empty
    /// vector (caller should fall back to defaults()).
    ///
    /// @param config_path  Path to the channel.conf file.
    /// @return             Parsed entries; may be empty on parse failure.
    [[nodiscard]] static std::vector<ChannelConfigEntry>
    load(const std::filesystem::path& config_path);

    /// Return a minimal set of hardcoded default channels.
    ///
    /// Used when no config file is available (e.g. in tests or early startup).
    /// Channels returned:
    ///   - "The Void"
    ///   - "Starcraft USA-1"
    ///   - "Diablo II"
    ///   - "Warcraft 3"
    ///   - "Chat"
    [[nodiscard]] static std::vector<ChannelConfigEntry> defaults();
};

}  // namespace pvpgn::infra::config
