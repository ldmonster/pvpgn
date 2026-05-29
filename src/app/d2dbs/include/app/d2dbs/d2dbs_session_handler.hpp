// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2dbs_session_handler.hpp
/// Application-layer bridge between the D2DBS protocol FSM and the domain
/// use cases.
///
/// ## Architecture
///
///   TCP bytes
///     │
///     ▼
///   D2DBSSessionFsm::feed()
///     │  D2DBSFsmCallbacks::on_*()
///     ▼
///   D2DBSSessionHandler          ← this class
///     │  invokes use case
///     │  calls ID2DBSSessionEgress::send_*()
///     ▼
///   ID2DBSSessionEgress (wire encoder / test mock)
///
/// ## Responsibilities
///
/// `D2DBSSessionHandler` is a **pure translation layer** — no business logic.
/// For each `D2DBSFsmCallbacks` callback it:
///   1. Extracts the relevant fields from the request struct.
///   2. Instantiates the appropriate domain use case (lightweight value object).
///   3. Calls `execute()` on the use case.
///   4. Calls the matching `ID2DBSSessionEgress::send_*()` method.
///
/// ## Callback wiring
///
///   on_char_save   → CharacterSaveUseCase   → egress.send_char_save_result()
///   on_char_load   → CharacterLoadUseCase   → egress.send_char_load_result()
///   on_char_ladder → LadderUpdateUseCase    → egress.send_ladder_update_result()
///   on_char_lock   → CharacterLockUseCase   → egress.send_char_login_result()
///                  → CharacterUnlockUseCase → egress.send_char_logout_result()
///   on_echo_reply  → no-op stub
///   on_disconnect  → no-op stub
///
/// ## C++20 conventions
///   - No exceptions; errors signalled via `std::optional` / `bool`
///   - `[[nodiscard]]` on factory functions
///   - `-std=c++20 -Wall -Wextra -Werror` clean

#include "app/d2dbs/d2dbs_session_egress.hpp"
#include "domain/d2dbs/character_save_repository.hpp"
#include "domain/d2dbs/ladder_repository.hpp"
#include "protocol/d2dbs/fsm.hpp"

namespace pvpgn::app::d2dbs {

// ---------------------------------------------------------------------------
// D2DBSSessionHandler
// ---------------------------------------------------------------------------

/// Bridges `D2DBSSessionFsm` callbacks to domain use cases.
///
/// Construct one handler per D2GS connection. The handler populates a
/// `D2DBSFsmCallbacks` struct (via `make_callbacks()`) that can be passed
/// directly to `D2DBSSessionFsm`.
///
/// Lifetime: `save_repo`, `ladder_repo`, and `egress` MUST outlive this
/// handler.
class D2DBSSessionHandler {
public:
    /// Construct the handler.
    ///
    /// @param save_repo    Character save repository (non-owning reference).
    /// @param ladder_repo  Ladder repository (non-owning reference).
    /// @param egress       Outbound response sink (non-owning reference).
    explicit D2DBSSessionHandler(
        domain::d2dbs::ICharacterSaveRepository& save_repo,
        domain::d2dbs::ID2DBSLadderRepository&   ladder_repo,
        ID2DBSSessionEgress&                     egress) noexcept;

    // Non-copyable, non-movable (holds references).
    D2DBSSessionHandler(const D2DBSSessionHandler&)            = delete;
    D2DBSSessionHandler& operator=(const D2DBSSessionHandler&) = delete;
    D2DBSSessionHandler(D2DBSSessionHandler&&)                 = delete;
    D2DBSSessionHandler& operator=(D2DBSSessionHandler&&)      = delete;

    ~D2DBSSessionHandler() = default;

    // -----------------------------------------------------------------------
    // Callback factory
    // -----------------------------------------------------------------------

    /// Build a `D2DBSFsmCallbacks` struct wired to this handler.
    ///
    /// Pass the returned struct to `D2DBSSessionFsm`'s constructor.
    /// The handler must outlive the FSM.
    [[nodiscard]] protocol::d2dbs::D2DBSFsmCallbacks make_callbacks();

private:
    domain::d2dbs::ICharacterSaveRepository& save_repo_;
    domain::d2dbs::ID2DBSLadderRepository&   ladder_repo_;
    ID2DBSSessionEgress&                     egress_;

    // -----------------------------------------------------------------------
    // Callback implementations
    // -----------------------------------------------------------------------

    /// SAVE_DATA_REQUEST (0x30) — persist character save data.
    core::Result<void, core::Error> handle_char_save(
        const protocol::d2dbs::D2DBSCharSaveData& req);

    /// GET_DATA_REQUEST (0x31) — load character save data.
    core::Result<void, core::Error> handle_char_load(
        const protocol::d2dbs::D2DBSCharLoadData& req);

    /// UPDATE_LADDER (0x32) — update ladder entry.
    core::Result<void, core::Error> handle_char_ladder(
        const protocol::d2dbs::D2DBSCharLadderData& req);

    /// CHAR_LOCK (0x33) — lock or unlock a character.
    ///
    /// Dispatches to lock or unlock based on `req.lockstatus`.
    core::Result<void, core::Error> handle_char_lock(
        const protocol::d2dbs::D2DBSCharLockReq& req);

    /// ECHO_REPLY (0x34) — keepalive pong from D2GS (no-op).
    core::Result<void, core::Error> handle_echo_reply(
        const protocol::d2dbs::D2DBSEchoReply& req);
};

} // namespace pvpgn::app::d2dbs
