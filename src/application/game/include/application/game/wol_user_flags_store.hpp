// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wol_user_flags_store.hpp
/// Port: per-account Westwood Online user option flags (findme / pageme).
///
/// WOL clients toggle whether they are findable (FINDUSER) and pageable (PAGE)
/// with the SETOPT command. The original stores these as per-connection flags,
/// both defaulting to ON, and FINDUSER/PAGE consult the TARGET user's flags. v3
/// keeps them in this small shared registry (keyed by AccountId) so one session
/// can honour another's SETOPT choices — exactly like [[peer_address_store]].
///
/// Kept apart from the Account aggregate (protocol-specific session state),
/// mirroring the other run-loop-scoped WOL stores.

#include "domain/shared/ids.hpp"

namespace pvpgn::application::game {

/// WOL find/page options. Both default ON, matching the original.
struct WolUserFlags {
    bool findme = true;
    bool pageme = true;
};

class IWolUserFlagsStore {
public:
    virtual ~IWolUserFlagsStore() = default;

    /// Set @p account's flags (SETOPT).
    virtual void set(domain::AccountId account, WolUserFlags flags) = 0;

    /// Get @p account's flags; returns the defaults (both ON) if never set.
    [[nodiscard]] virtual WolUserFlags
    get(domain::AccountId account) const = 0;

    /// Drop @p account's entry (on disconnect). No-op if absent.
    virtual void remove(domain::AccountId account) = 0;
};

}  // namespace pvpgn::application::game
