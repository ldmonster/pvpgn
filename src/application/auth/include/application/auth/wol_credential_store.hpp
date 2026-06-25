// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wol_credential_store.hpp
/// Port for the Westwood Online (WOL) password-token store.
///
/// WOL clients (C&C, Red Alert, Tiberian Sun, Nox, Dune 2000, Emperor, …)
/// authenticate with an opaque "APGAR" token sent in the WOL/IRC handshake
/// (`APGAR <token>`). The original server stores this token per account and
/// compares it verbatim (a plain string compare — it does NOT re-derive the
/// Westwood hash), auto-creating the account on first login. See
/// `pvpgn-server` `handle_wol.cpp` `handle_wol_authenticate`.
///
/// Kept separate from IAccountRepository — exactly like [[srp3_credential_store]]
/// — so the Account aggregate is not burdened with protocol-specific fields.

#include <optional>
#include <string>
#include <string_view>

#include "domain/shared/ids.hpp"

namespace pvpgn::application::auth {

/// WOL credentials persisted per account.
struct WolCredentials {
    std::string       apgar;          ///< opaque Westwood password token
    domain::AccountId account_id{0};
};

/// Port: stores and looks up WOL APGAR tokens by account name.
class IWolCredentialStore {
public:
    virtual ~IWolCredentialStore() = default;

    /// Look up the WOL token for `username` (case-insensitive).
    /// Returns std::nullopt if the account has no WOL token on file.
    [[nodiscard]] virtual std::optional<WolCredentials>
    find(std::string_view username) const = 0;

    /// Store (or replace) the WOL token for `username` (case-insensitive).
    virtual void store(std::string_view username,
                       const WolCredentials& creds) = 0;
};

}  // namespace pvpgn::application::auth
