// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ids.hpp
/// Strong-typed identifiers for cross-aggregate references.
/// Aggregates **must** refer to one another by these IDs — never by
/// pointer aliases — so persistence boundaries and event payloads stay
/// trivially serialisable.

#include <cstdint>

#include "core/strong_typedef.hpp"

namespace pvpgn::domain {

using AccountId = core::StrongId<struct AccountIdTag, std::uint32_t>;
using ChannelId = core::StrongId<struct ChannelIdTag, std::uint32_t>;
using GameId    = core::StrongId<struct GameIdTag,    std::uint32_t>;
using ClanId    = core::StrongId<struct ClanIdTag,    std::uint32_t>;
using TeamId    = core::StrongId<struct TeamIdTag,    std::uint32_t>;
using SessionId = core::StrongId<struct SessionIdTag, std::uint64_t>;

}  // namespace pvpgn::domain
