// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file srp3_credential_store.hpp
/// Port for the WarCraft III SRP-3 credential store.
///
/// WAR3/W3XP authenticate with the legacy Battle.net SRP-3 scheme (32-byte
/// modulus), NOT the 128-byte SRP-6a abstraction used by [[login_user_nls]].
/// Each account stores a 32-byte random salt and a 32-byte password verifier
/// (v = g^x mod N), exactly as the original server persists
/// `BNET\\acct\\nls_salt` / `BNET\\acct\\nls_verifier`. The server never sees
/// the password.

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "domain/shared/ids.hpp"

namespace pvpgn::application::auth {

/// SRP-3 credentials persisted per account (WAR3/W3XP).
struct Srp3Credentials {
    std::array<std::uint8_t, 32> salt{};      ///< random salt (s), raw bytes
    std::array<std::uint8_t, 32> verifier{};  ///< v = g^x mod N, wire bytes
    domain::AccountId            account_id{0};
};

/// Port: stores and looks up SRP-3 credentials by account name.
///
/// Separate from IAccountRepository so the Account aggregate is not burdened
/// with SRP-specific fields. Implementations may back it with the legacy
/// attribute bag, a SQL table, or an in-memory map for tests.
class ISrp3CredentialStore {
public:
    virtual ~ISrp3CredentialStore() = default;

    /// Look up SRP-3 credentials for `username` (case-insensitive).
    /// Returns std::nullopt if the account has no SRP-3 credentials.
    [[nodiscard]] virtual std::optional<Srp3Credentials>
    find(std::string_view username) const = 0;

    /// Store (or replace) SRP-3 credentials for `username` (case-insensitive).
    virtual void store(std::string_view username,
                       const Srp3Credentials& creds) = 0;
};

}  // namespace pvpgn::application::auth
