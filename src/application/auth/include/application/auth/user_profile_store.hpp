// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file user_profile_store.hpp
/// Port for per-account string attributes (the BNCS "user data" / profile bag).
///
/// BNCS clients read and write free-form account attributes via SID_READUSERDATA
/// (0x26, STATSREQ) and SID_WRITEUSERDATA (0x27, STATSUPDATE) — the classic
/// `profile\sex`, `profile\age`, `profile\location`, `profile\description` fields,
/// plus assorted `record\...` / `System\...` keys. The original keeps these as
/// per-account string attributes (account_get_strattr / account_set_strattr).
///
/// Kept apart from the Account aggregate (free-form protocol attributes), like
/// the other run-loop-scoped stores ([[srp3_credential_store]] etc.). Keyed by
/// account name (case-insensitive) so READUSERDATA can read any account's
/// profile while WRITEUSERDATA only mutates the caller's own.

#include <optional>
#include <string>
#include <string_view>

namespace pvpgn::application::auth {

class IUserProfileStore {
public:
    virtual ~IUserProfileStore() = default;

    /// Store (or replace) @p key = @p value for @p account (case-insensitive
    /// account name; key stored verbatim).
    virtual void set(std::string_view account, std::string_view key,
                     std::string_view value) = 0;

    /// Look up @p key for @p account; nullopt if unset.
    [[nodiscard]] virtual std::optional<std::string>
    get(std::string_view account, std::string_view key) const = 0;
};

}  // namespace pvpgn::application::auth
