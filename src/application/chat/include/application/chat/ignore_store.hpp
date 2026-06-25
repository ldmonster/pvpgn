// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ignore_store.hpp
/// Port for the per-account ignore ("squelch") list.
///
/// A user who /squelches another stops receiving that user's channel messages.
/// The check is needed on the *broadcast* side: when fanning a TALK/EMOTE out to
/// a channel, a recipient who ignores the sender is dropped from the recipient
/// set (mirroring the original's MF_X "player is ignored" delivery filter).
///
/// Kept separate from the account aggregate, like the SRP-3 / WOL / friend
/// stores, so the aggregate is not burdened with protocol-specific state.

#include "domain/shared/ids.hpp"

namespace pvpgn::application::chat {

class IIgnoreStore {
public:
    virtual ~IIgnoreStore() = default;

    /// Add `target` to `owner`'s ignore list. Returns true if newly added.
    virtual bool squelch(domain::AccountId owner, domain::AccountId target) = 0;

    /// Remove `target` from `owner`'s ignore list. Returns true if it was
    /// present and removed.
    virtual bool unsquelch(domain::AccountId owner, domain::AccountId target) = 0;

    /// True if `owner` is currently ignoring `target`.
    [[nodiscard]] virtual bool
    ignores(domain::AccountId owner, domain::AccountId target) const = 0;
};

}  // namespace pvpgn::application::chat
