// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ban.hpp
/// `Ban` value object — used inside `identity::Account` and shared with
/// `moderation` once that context lands. Pure: no I/O, no scheduler,
/// uses `core::IClock` to decide expiry on demand.

#include <optional>
#include <string>

#include "core/clock.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain {

enum class BanScope : std::uint8_t {
    Account,
    Ip,
    IpRange,
};

struct Ban {
    BanScope                                scope;
    std::string                             reason;
    AccountId                               issuer;
    core::SystemTime                        issued_at;
    std::optional<core::SystemTime>         expires_at;

    /// True if this ban is still effective at the given wall-clock time.
    bool active_at(core::SystemTime now) const noexcept {
        return !expires_at.has_value() || now < *expires_at;
    }
};

}  // namespace pvpgn::domain
