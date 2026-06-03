// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
//
// domain/identity/ports.hpp — Abstract ports (interfaces) for the identity bounded context.
// Implementations live in src/infra/<tech>/ and src/integration/<binding>/.
// Plan 05: Ports Consolidation (migrated from application/ports/)

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"

namespace pvpgn::domain::identity {

// ---------------------------------------------------------------------------
// IAccountRepository — segregated into reader + writer ports (ADR 0012 / ISP).
// Read-only consumers (e.g. the permission checker) depend on IAccountReader
// only; IAccountRepository = IAccountReader + IAccountWriter remains for
// callers and implementers that need both, so existing code is unaffected.
// ---------------------------------------------------------------------------

/// Read side of the account repository.
class IAccountReader {
public:
    virtual ~IAccountReader() = default;

    [[nodiscard]] virtual core::Result<Account>
    find_by_id(domain::AccountId id) const = 0;

    [[nodiscard]] virtual core::Result<Account>
    find_by_name(const domain::UserName& name) const = 0;

    virtual void forEach(
        std::function<bool(const Account&)> predicate) const = 0;

    [[nodiscard]] virtual std::size_t size() const noexcept = 0;

protected:
    IAccountReader() = default;
};

/// Write side of the account repository.
class IAccountWriter {
public:
    virtual ~IAccountWriter() = default;

    virtual core::Status<> save(const Account& account) = 0;
    virtual core::Status<> remove(domain::AccountId id) = 0;

protected:
    IAccountWriter() = default;
};

/// Full account repository: read + write. Implementers derive from this and
/// override all six methods exactly as before.
class IAccountRepository : public IAccountReader, public IAccountWriter {
public:
    IAccountRepository(const IAccountRepository&)            = delete;
    IAccountRepository& operator=(const IAccountRepository&) = delete;
    IAccountRepository(IAccountRepository&&)                 = delete;
    IAccountRepository& operator=(IAccountRepository&&)      = delete;
    ~IAccountRepository() override                           = default;

protected:
    IAccountRepository() = default;
};

// ---------------------------------------------------------------------------
// IPasswordHasher
// ---------------------------------------------------------------------------

/// Port for deriving the legacy Battle.net session hash:
///   `hash2 = bnet_hash(client_token || server_token || hash1)`
/// where `hash1` is the persisted password digest.
///
/// The application layer uses this abstraction so that
/// `LoginUser` / `ChangePassword` never depend on legacy crypto
/// internals. Adapters live in `infra/legacy_crypto/`.
class IPasswordHasher {
public:
    virtual ~IPasswordHasher() = default;

    /// Derive the per-connection session hash from a stored password
    /// hash and the (ticks, sessionkey) pair negotiated at logon.
    [[nodiscard]] virtual domain::BNHash
    derive_session_hash(const domain::BNHash& password_hash1,
                        std::uint32_t         ticks,
                        std::uint32_t         sessionkey) const noexcept = 0;
};

// ---------------------------------------------------------------------------
// ISessionRegistry
// ---------------------------------------------------------------------------

class ISessionRegistry {
public:
    virtual ~ISessionRegistry() = default;

    ISessionRegistry(const ISessionRegistry&)            = delete;
    ISessionRegistry& operator=(const ISessionRegistry&) = delete;
    ISessionRegistry(ISessionRegistry&&)                 = delete;
    ISessionRegistry& operator=(ISessionRegistry&&)      = delete;

    virtual core::Status<> attach(domain::SessionId session,
                                  domain::AccountId account) = 0;

    virtual void detach(domain::SessionId session) = 0;

    [[nodiscard]] virtual std::optional<domain::SessionId>
    session_for(domain::AccountId account) const = 0;

    [[nodiscard]] virtual std::optional<domain::AccountId>
    account_for(domain::SessionId session) const = 0;

    [[nodiscard]] virtual std::vector<domain::SessionId> list() const = 0;

protected:
    ISessionRegistry() = default;
};

// ---------------------------------------------------------------------------
// ISessionTokenIssuer
// ---------------------------------------------------------------------------

class ISessionTokenIssuer {
public:
    virtual ~ISessionTokenIssuer() = default;

    /// Issue a fresh opaque session token bound to `account_id`.
    virtual std::string issue(domain::AccountId account_id) = 0;

    /// Look up the account that owns `token`. Returns NotFound if the
    /// token is unknown or has been revoked.
    [[nodiscard]] virtual core::Result<domain::AccountId>
        validate(std::string_view token) = 0;

    /// Revoke `token`. No-op if the token is unknown.
    virtual void revoke(std::string_view token) noexcept = 0;
};

} // namespace pvpgn::domain::identity
