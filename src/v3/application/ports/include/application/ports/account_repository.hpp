// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_repository.hpp
/// Persistence port for the `identity::Account` aggregate.
///
/// The repository is the *only* seam through which the application
/// layer reads or writes accounts. Implementations live in `infra/`:
/// in-memory (tests + dev), file-backed (legacy parity), and SQL
/// (production) all satisfy this interface.

#include <cstddef>
#include <optional>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"

namespace pvpgn::application::ports {

class IAccountRepository {
public:
    virtual ~IAccountRepository() = default;

    /// Look up an account by primary key. Returns `NotFound` if absent.
    virtual core::Result<domain::identity::Account>
    find_by_id(domain::AccountId id) const = 0;

    /// Case-insensitive name lookup. Returns `NotFound` if absent.
    virtual core::Result<domain::identity::Account>
    find_by_name(const domain::UserName& name) const = 0;

    /// Upsert. Implementations are expected to be idempotent on
    /// repeated `save()` of the same logical state.
    virtual core::Status<>
    save(const domain::identity::Account& account) = 0;

    /// Remove. Returns `NotFound` if the id doesn't exist.
    virtual core::Status<> remove(domain::AccountId id) = 0;

    virtual std::size_t size() const noexcept = 0;
};

}  // namespace pvpgn::application::ports
