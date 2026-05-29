// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file icon_provider.hpp
/// Port: client-icon lookup and raw icon-table provider.

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace pvpgn::application::ports {

/// Port: icon provider interface for hexagonal architecture.
/// Implementations load icon data from the icons.conf file or a database.
class IIconProvider {
public:
    virtual ~IIconProvider() = default;

    /// Return the icon tag (4-byte product tag string) for the given client
    /// tag, or `std::nullopt` on a cache miss.
    virtual std::optional<std::string>
    icon_for(std::string_view client_tag) const = 0;

    /// Return the full icon table as raw bytes suitable for a BNCS
    /// SID_ICONS response payload.
    virtual std::span<const std::byte>
    raw_icon_data() const = 0;
};

}  // namespace pvpgn::application::ports
