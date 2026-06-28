// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2dbs_session_egress.hpp
/// Egress interface for D2DBS session responses.
///
/// `ID2DBSSessionEgress` is the outbound port of the `D2DBSSessionHandler`.
/// It abstracts the wire-level encoding and TCP send so that the handler
/// can be unit-tested without a real socket.
///
/// ## Design
///
/// The handler calls these methods after executing a domain use case.
/// The production implementation will encode the result into the
/// appropriate D2DBS wire packet and write it to the TCP stream.
/// The test implementation captures the calls for assertion.
///
/// ## Naming convention
///
/// Method names mirror the D2DBS reply packet names from `codec.hpp`:
///   send_char_save_result   → SAVE_DATA_REPLY (0x30)
///   send_char_load_result   → GET_DATA_REPLY  (0x31)
///   send_char_login_result  → lock acquired (CHAR_LOCK response)
///   send_char_logout_result → lock released (CHAR_LOCK response)
///   send_ladder_update_result → UPDATE_LADDER acknowledgement

#include "domain/d2dbs/types.hpp"

namespace pvpgn::app::d2dbs {

// ---------------------------------------------------------------------------
// ID2DBSSessionEgress
// ---------------------------------------------------------------------------

/// Pure-virtual outbound port for D2DBS session responses.
///
/// Implementations must be non-throwing; all error handling is done
/// internally (e.g. by logging and dropping the packet).
///
/// Lifetime: the implementation must outlive the `D2DBSSessionHandler`
/// that holds a reference to it.
class ID2DBSSessionEgress {
public:
    virtual ~ID2DBSSessionEgress() = default;

    /// Send the result of a character login (lock acquisition).
    ///
    /// Called after processing a CHAR_LOCK request with lockstatus != 0.
    ///
    /// @param success  `true` if the lock was acquired successfully.
    virtual void send_char_login_result(bool success) = 0;

    /// Send the result of a character logout (lock release).
    ///
    /// Called after processing a CHAR_LOCK request with lockstatus == 0.
    ///
    /// @param success  `true` if the lock was released successfully.
    virtual void send_char_logout_result(bool success) = 0;

    /// Send the result of a character save (SAVE_DATA_REPLY / 0x30).
    ///
    /// @param success  `true` if the save was persisted successfully.
    virtual void send_char_save_result(bool success) = 0;

    /// Send the result of a character load (GET_DATA_REPLY / 0x31).
    ///
    /// @param success  `true` if the character data was found and loaded.
    /// @param data     Pointer to the loaded save data (non-null iff success).
    virtual void send_char_load_result(
        bool success,
        const domain::d2dbs::CharacterSaveData* data) = 0;

    /// Send the result of a ladder update acknowledgement.
    ///
    /// Called after processing an UPDATE_LADDER packet.
    ///
    /// @param success  `true` if the ladder entry was updated successfully.
    virtual void send_ladder_update_result(bool success) = 0;
};

} // namespace pvpgn::app::d2dbs
