// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2cs_session_egress.hpp
/// Egress interface for D2CS session responses.
///
/// `ID2CSSessionEgress` is the outbound port of the `D2CSSessionHandler`.
/// It abstracts the wire-level encoding and TCP send so that the handler
/// can be unit-tested without a real socket.
///
/// ## Design
///
/// The handler calls these methods after executing a domain use case.
/// The production implementation will encode the result into the
/// appropriate D2CS wire packet and write it to the TCP stream.
/// The test implementation captures the calls for assertion.
///
/// ## Naming convention
///
/// Method names mirror the D2CS reply packet names from `wire_types.hpp`:
///   send_char_list        → CHARLISTREPLY (0x17)
///   send_char_select_result → CHARLOGINREPLY (0x07)
///   send_char_create_result → CREATECHARREPLY (0x02)
///   send_char_delete_result → DELETECHARREPLY (0x0A)
///   send_ladder           → LADDERREPLY (0x11)
///   send_realm_logon_result → LOGINREPLY (0x01)

#include <vector>

#include "domain/d2cs/types.hpp"

namespace pvpgn::app::d2cs {

// ---------------------------------------------------------------------------
// ID2CSSessionEgress
// ---------------------------------------------------------------------------

/// Pure-virtual outbound port for D2CS session responses.
///
/// Implementations must be non-throwing; all error handling is done
/// internally (e.g. by logging and dropping the packet).
///
/// Lifetime: the implementation must outlive the `D2CSSessionHandler`
/// that holds a reference to it.
class ID2CSSessionEgress {
public:
    virtual ~ID2CSSessionEgress() = default;

    /// Send the character list for an account (CHARLISTREPLY / 0x17).
    ///
    /// @param chars  All characters belonging to the account (may be empty).
    virtual void send_char_list(
        const std::vector<domain::d2cs::CharacterInfo>& chars) = 0;

    /// Send the result of a character-list request (success/failure flag).
    ///
    /// Used when the repository returns `std::nullopt` (error path).
    ///
    /// @param success  `true` if the list was retrieved successfully.
    virtual void send_char_list_result(bool success) = 0;

    /// Send the result of a character-select (CHARLOGINREPLY / 0x07).
    ///
    /// @param success  `true` if the character was found.
    /// @param info     Pointer to the found character (non-null iff success).
    virtual void send_char_select_result(
        bool success,
        const domain::d2cs::CharacterInfo* info) = 0;

    /// Send the result of a character-create (CREATECHARREPLY / 0x02).
    ///
    /// @param success  `true` if the character was created.
    virtual void send_char_create_result(bool success) = 0;

    /// Send the result of a character-delete (DELETECHARREPLY / 0x0A).
    ///
    /// @param success  `true` if the character was deleted.
    virtual void send_char_delete_result(bool success) = 0;

    /// Send a page of ladder entries (LADDERREPLY / 0x11).
    ///
    /// @param entries  Ladder entries sorted by rank ascending (may be empty).
    virtual void send_ladder(
        const std::vector<domain::d2cs::LadderEntry>& entries) = 0;

    /// Send the result of a realm logon (LOGINREPLY / 0x01).
    ///
    /// @param result  The logon result code.
    virtual void send_realm_logon_result(
        domain::d2cs::RealmLogonResult result) = 0;
};

} // namespace pvpgn::app::d2cs
