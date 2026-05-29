// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2cs_session_handler.hpp
/// Application-layer bridge between the D2CS protocol FSM and the domain
/// use cases.
///
/// ## Architecture
///
///   TCP bytes
///     │
///     ▼
///   D2CSSessionFsm::feed()
///     │  D2CSFsmCallbacks::on_*()
///     ▼
///   D2CSSessionHandler          ← this class
///     │  invokes use case
///     │  calls ID2CSSessionEgress::send_*()
///     ▼
///   ID2CSSessionEgress (wire encoder / test mock)
///
/// ## Responsibilities
///
/// `D2CSSessionHandler` is a **pure translation layer** — no business logic.
/// For each `D2CSFsmCallbacks` callback it:
///   1. Extracts the relevant fields from the request struct.
///   2. Instantiates the appropriate domain use case (lightweight value object).
///   3. Calls `execute()` on the use case.
///   4. Calls the matching `ID2CSSessionEgress::send_*()` method.
///
/// ## Callback wiring
///
///   on_login          → stub: egress.send_realm_logon_result(Success)
///   on_char_list      → CharacterListUseCase  → egress.send_char_list()
///   on_char_list_110  → CharacterListUseCase  → egress.send_char_list()
///   on_char_login     → CharacterSelectUseCase → egress.send_char_select_result()
///   on_create_char    → CharacterCreateUseCase → egress.send_char_create_result()
///   on_delete_char    → CharacterDeleteUseCase → egress.send_char_delete_result()
///   on_ladder         → LadderQueryUseCase    → egress.send_ladder()
///   on_char_ladder    → LadderQueryUseCase (char lookup) → egress.send_ladder()
///   on_motd           → no-op stub
///   on_create_game    → no-op stub
///   on_join_game      → no-op stub
///   on_game_list      → no-op stub
///   on_game_info      → no-op stub
///   on_cancel_create_game → no-op stub
///   on_convert_char   → no-op stub
///   on_disconnect     → no-op stub
///
/// ## C++20 conventions
///   - No exceptions; errors signalled via `std::optional` / `bool`
///   - `[[nodiscard]]` on factory functions
///   - `-std=c++20 -Wall -Wextra -Werror` clean

#include <functional>

#include "app/d2cs/d2cs_session_egress.hpp"
#include "domain/d2cs/character_repository.hpp"
#include "domain/d2cs/ladder_repository.hpp"
#include "protocol/d2cs/fsm.hpp"

namespace pvpgn::app::d2cs {

// ---------------------------------------------------------------------------
// D2CSSessionHandler
// ---------------------------------------------------------------------------

/// Bridges `D2CSSessionFsm` callbacks to domain use cases.
///
/// Construct one handler per client session. The handler populates a
/// `D2CSFsmCallbacks` struct (via `make_callbacks()`) that can be passed
/// directly to `D2CSSessionFsm`.
///
/// Lifetime: `char_repo`, `ladder_repo`, and `egress` MUST outlive this
/// handler.
class D2CSSessionHandler {
public:
    /// Construct the handler.
    ///
    /// @param char_repo   Character repository (non-owning reference).
    /// @param ladder_repo Ladder repository (non-owning reference).
    /// @param egress      Outbound response sink (non-owning reference).
    explicit D2CSSessionHandler(
        domain::d2cs::ICharacterRepository& char_repo,
        domain::d2cs::ILadderRepository&    ladder_repo,
        ID2CSSessionEgress&                 egress) noexcept;

    // Non-copyable, non-movable (holds references).
    D2CSSessionHandler(const D2CSSessionHandler&)            = delete;
    D2CSSessionHandler& operator=(const D2CSSessionHandler&) = delete;
    D2CSSessionHandler(D2CSSessionHandler&&)                 = delete;
    D2CSSessionHandler& operator=(D2CSSessionHandler&&)      = delete;

    ~D2CSSessionHandler() = default;

    // -----------------------------------------------------------------------
    // Callback factory
    // -----------------------------------------------------------------------

    /// Build a `D2CSFsmCallbacks` struct wired to this handler.
    ///
    /// Pass the returned struct to `D2CSSessionFsm`'s constructor.
    /// The handler must outlive the FSM.
    [[nodiscard]] protocol::d2cs::D2CSFsmCallbacks make_callbacks();

private:
    domain::d2cs::ICharacterRepository& char_repo_;
    domain::d2cs::ILadderRepository&    ladder_repo_;
    ID2CSSessionEgress&                 egress_;

    // -----------------------------------------------------------------------
    // Callback implementations
    // -----------------------------------------------------------------------

    /// LOGINREQ (0x01) — stub: always succeeds (real auth in later round).
    core::Result<void, core::Error> handle_login(
        const protocol::d2cs::D2CSLoginRequest& req);

    /// CHARLOGINREQ (0x07) — character select.
    core::Result<void, core::Error> handle_char_login(
        const protocol::d2cs::D2CSCharLoginRequest& req);

    /// CREATECHARREQ (0x02) — character create.
    core::Result<void, core::Error> handle_create_char(
        const protocol::d2cs::D2CSCreateCharRequest& req);

    /// DELETECHARREQ (0x0A) — character delete.
    core::Result<void, core::Error> handle_delete_char(
        const protocol::d2cs::D2CSDeleteCharRequest& req);

    /// CHARLISTREQ (0x17) — character list.
    core::Result<void, core::Error> handle_char_list(
        const protocol::d2cs::D2CSCharListRequest& req);

    /// CHARLISTREQ110 (0x19) — character list (1.10+ variant).
    core::Result<void, core::Error> handle_char_list_110(
        const protocol::d2cs::D2CSCharListRequest& req);

    /// LADDERREQ (0x11) — ladder page query.
    core::Result<void, core::Error> handle_ladder(
        const protocol::d2cs::D2CSLadderRequest& req);

    /// CHARLADDERREQ (0x16) — character-specific ladder query.
    core::Result<void, core::Error> handle_char_ladder(
        const protocol::d2cs::D2CSCharLadderRequest& req);

    // -----------------------------------------------------------------------
    // No-op stubs (wired but not yet implemented)
    // -----------------------------------------------------------------------

    core::Result<void, core::Error> handle_create_game(
        const protocol::d2cs::D2CSCreateGameRequest& req);

    core::Result<void, core::Error> handle_join_game(
        const protocol::d2cs::D2CSJoinGameRequest& req);

    core::Result<void, core::Error> handle_game_list(
        const protocol::d2cs::D2CSGameListRequest& req);

    core::Result<void, core::Error> handle_game_info(
        const protocol::d2cs::D2CSGameInfoRequest& req);

    core::Result<void, core::Error> handle_motd(
        const protocol::d2cs::D2CSMotdRequest& req);

    core::Result<void, core::Error> handle_cancel_create_game();

    core::Result<void, core::Error> handle_convert_char(
        const protocol::d2cs::D2CSConvertCharRequest& req);

    void handle_disconnect();

    // -----------------------------------------------------------------------
    // Session state (account name set on login)
    // -----------------------------------------------------------------------

    /// Account name set when on_login fires; used by subsequent callbacks.
    std::string account_name_;
};

} // namespace pvpgn::app::d2cs
