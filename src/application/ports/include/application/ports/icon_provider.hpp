// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file icon_provider.hpp
/// Port for client-tag → icon-tag lookups and raw icon table bytes.

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace pvpgn::application::ports {

class IIconProvider {
public:
    virtual ~IIconProvider() = default;

    /// Return the icon tag for `client_tag`, or `std::nullopt` if unknown.
    [[nodiscard]] virtual std::optional<std::string>
        icon_for(std::string_view client_tag) const = 0;

    /// Return the raw bytes of the icons table (e.g. icons.bni contents).
    [[nodiscard]] virtual std::span<const std::byte>
        raw_icon_data() const = 0;
};

} // namespace pvpgn::application::ports
