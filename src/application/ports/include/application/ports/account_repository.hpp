// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_repository.hpp
/// Application-layer port for the account store.

#include <cstddef>
#include <functional>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"

namespace pvpgn::application::ports {

class IAccountRepository {
public:
    virtual ~IAccountRepository() = default;

    IAccountRepository(const IAccountRepository&)            = delete;
    IAccountRepository& operator=(const IAccountRepository&) = delete;
    IAccountRepository(IAccountRepository&&)                 = delete;
    IAccountRepository& operator=(IAccountRepository&&)      = delete;

    [[nodiscard]] virtual core::Result<domain::identity::Account>
    find_by_id(domain::AccountId id) const = 0;

    [[nodiscard]] virtual core::Result<domain::identity::Account>
    find_by_name(const domain::UserName& name) const = 0;

    virtual core::Status<>
    save(const domain::identity::Account& account) = 0;

    virtual core::Status<>
    remove(domain::AccountId id) = 0;

    virtual void forEach(
        std::function<bool(const domain::identity::Account&)> predicate) const = 0;

    [[nodiscard]] virtual std::size_t size() const noexcept = 0;

protected:
    IAccountRepository() = default;
};

} // namespace pvpgn::application::ports
