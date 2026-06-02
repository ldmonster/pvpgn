// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file icon_provider.hpp
/// Application-layer port for client-icon lookup.
///
/// Abstracts the source of per-client-tag icons and the raw icon table so the
/// application layer does not depend on a concrete icon-file backend.

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace pvpgn::application::ports {

/// Provides client icons keyed by client tag plus the raw icon table.
class IIconProvider {
public:
    virtual ~IIconProvider() = default;

    /// Return the icon tag registered for `client_tag`, if any.
    [[nodiscard]] virtual std::optional<std::string>
    icon_for(std::string_view client_tag) const = 0;

    /// Return the raw bytes of the icon table.
    [[nodiscard]] virtual std::span<const std::byte> raw_icon_data() const = 0;
};

}  // namespace pvpgn::application::ports
