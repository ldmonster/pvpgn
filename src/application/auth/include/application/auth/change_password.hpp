// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file change_password.hpp
/// Use-case: rotate an account's password after verifying the
/// caller knows the current one. Mirrors the legacy
/// `CLIENT_CHANGEPASSWDREQ` flow but with the bnetd-side mutation
/// expressed purely against the v3 domain aggregate.
///
/// The use-case is pure: no globals, no clocks; collaborators are
/// reference-injected. Successful rotation:
///   * mutates `identity::Account` via `change_password()`
///   * which drains `AccountPasswordChanged` (always) and
///     `AccountPasswordRotationCleared` (iff the rotation flag had
///     been set);
///   * the use-case persists the account via `IAccountRepository`
///     and pushes every drained event to the `IEventBus`.
///
/// Batch 27d: green-field code, not yet called by any legacy bridge.
/// A follow-up batch will wire `CLIENT_CHANGEPASSWDREQ` to this.

#include <cstdint>

#include "domain/identity/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/identity/ports.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/user_name.hpp"

namespace pvpgn::application::auth {

struct ChangePasswordRequest {
    domain::UserName name;
    domain::BNHash   current_password;
    domain::BNHash   new_password;
};

/// Legacy `CLIENT_CHANGEPASSREQ` arm: the client never sends
/// `current_password` (hash1) directly; instead it sends a session-
/// hash derived from (ticks || sessionkey || hash1). The bridge
/// fills this struct from the packet body and the use-case
/// re-derives via `IPasswordHasher`.
struct ChangePasswordWithSessionHashRequest {
    domain::UserName name;
    /// Client-supplied `hash2`.
    domain::BNHash   current_password_hash2;
    std::uint32_t    ticks;
    std::uint32_t    sessionkey;
    domain::BNHash   new_password;
};

enum class ChangePasswordError {
    UnknownUser,
    InvalidCurrentPassword,
    /// New password equals the current one (no-op). Domain treats
    /// this as a successful rotation, but bnetd parity expects us
    /// to surface it so the client receives a soft "no change"
    /// reply instead of "ok".
    PasswordUnchanged,
    PersistenceFailed,
    Internal,
};

class ChangePasswordUseCase {
public:
    ChangePasswordUseCase(domain::identity::IAccountRepository& accounts,
                          application::ports::IEventBus& bus) noexcept
        : accounts_(accounts), bus_(bus), hasher_(nullptr) {}

    /// Hash2-aware overload (Batch 29a). When constructed with a
    /// hasher the use-case can also accept
    /// `ChangePasswordWithSessionHashRequest`. The cleartext-hash1
    /// overload remains available regardless.
    ChangePasswordUseCase(domain::identity::IAccountRepository& accounts,
                          application::ports::IEventBus& bus,
                          const domain::identity::IPasswordHasher& hasher) noexcept
        : accounts_(accounts), bus_(bus), hasher_(&hasher) {}

    using Result = core::Result<domain::AccountId, ChangePasswordError>;

    Result execute(const ChangePasswordRequest& req);

    /// Hash2-arm execution. Requires a hasher (the 2-arg ctor
    /// returns `Internal` to make missing-hasher misuse loud).
    Result execute(const ChangePasswordWithSessionHashRequest& req);

private:
    domain::identity::IAccountRepository&    accounts_;
    application::ports::IEventBus&             bus_;
    const domain::identity::IPasswordHasher* hasher_;
};

}  // namespace pvpgn::application::auth
