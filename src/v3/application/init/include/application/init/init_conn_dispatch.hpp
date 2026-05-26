// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file init_conn_dispatch.hpp
/// V3 owner of the byte-1 connection-class decision that the legacy
/// `handle_init_packet` performs (see `src/bnetd/handle_init.cpp`).
///
/// The legacy flow:
///   1. Client opens a TCP socket to bnetd and sends a single
///      "magic" byte naming the service it wants
///      (`CLIENT_INITCONN_CLASS_*` in `init_protocol.h`).
///   2. Legacy `handle_init_packet` switches on that byte and
///      transitions the `t_connection` into the matching class
///      (`conn_class_bnet`, `conn_class_file`, etc.), with a few
///      side-policies: max-connections-per-IP gate (skipped for
///      `D2CS_BNETD`), an IP allow-list check for `D2CS_BNETD`,
///      and an explicit reject of `ENC` / `LOCALMACHINE`.
///
/// This v3 module owns step 2 *minus* the legacy-only side-effects
/// (max-conns, realmlist lookup, `conn_set_state` / `conn_set_class`,
/// `handle_d2cs_init`). The result of `dispatch_init_conn` tells the
/// caller which connection class the client requested, or that the
/// request must be rejected. The caller -- a strangler bridge or,
/// eventually, a v3 acceptor -- is responsible for applying that
/// decision to whatever connection abstraction it owns.
///
/// Pure-function, constexpr, header-only.

#include <cstdint>

#include "protocol/bnet/init_wire_types.hpp"

namespace pvpgn::application::init {

/// One value per accepted `CLIENT_INITCONN_CLASS_*`, plus a single
/// `kRejected` sink for every unsupported / unknown byte. The
/// integer values are stable and form part of the C ABI of any
/// bridge that uses this enum -- do not renumber.
enum class InitDecision : std::uint8_t {
    kBnet      = 0,   ///< Standard Battle.net session.
    kFile      = 1,   ///< BNFTP file transfer connection.
    kBot       = 2,   ///< Chat-bot connection.
    kTelnet    = 3,   ///< Telnet-style ASCII connection.
    kD2csBnetd = 4,   ///< D2CS<->bnetd realm link. IP allow-list applies.
    kRateLimited = 5, ///< R168.b: per-IP cap exceeded; reject without state change.
    kD2csIpDenied = 6,///< R168.c: D2CS_BNETD client whose IP is not in the realmlist.
    kRejected  = 0xff ///< Unknown or explicitly-unsupported class byte.
};

/// Input: the verbatim first byte the client sent after TCP connect,
/// plus the per-IP rate-limit context (R168.b). The rate-limit
/// fields are optional -- a default-constructed `InitConnRequest`
/// (or one with `max_conns_per_ip == 0`) skips the limit check.
struct InitConnRequest {
    std::uint8_t cclass = 0;
    /// Current open connections from the same client IP, NOT
    /// counting the one being dispatched. Caller-supplied.
    unsigned int conn_count = 0;
    /// Configured cap (`prefs.max_conns_per_IP`); 0 disables the
    /// check. The legacy code skips this gate for D2CS_BNETD;
    /// v3 reproduces that exception.
    unsigned int max_conns_per_ip = 0;
    /// R168.c: for `kClassD2csBnetd` requests, whether the client
    /// IP appears in the realmlist. Default `true` preserves the
    /// behavior of older callers that did not supply this field;
    /// new v3 acceptors should pass the realmlist verdict.
    bool d2cs_ip_allowed = true;

    constexpr bool operator==(const InitConnRequest&) const = default;
};

/// Output: the dispatch decision for the caller to enact.
struct InitConnResponse {
    InitDecision decision = InitDecision::kRejected;

    constexpr bool operator==(const InitConnResponse&) const = default;
};

/// Map a connection-class byte to a v3 dispatch decision. Pure
/// function with no observable side effect.
///
/// Notes on the legacy mapping:
///   * `CLIENT_INITCONN_CLASS_ENC` (0x04) -- the legacy code logs
///     and closes; v3 reports `kRejected`.
///   * `CLIENT_INITCONN_CLASS_LOCALMACHINE` (0x98) -- the legacy
///     code path is commented out and immediately returns -1; v3
///     reports `kRejected`.
///   * `CLIENT_INITCONN_CLASS_D2CS` / `D2GS` (0x01 / 0x64) are
///     handled by `d2cs`'s own `handle_init_packet`, not bnetd's,
///     so they are intentionally absent here. (`kClassBnet` and
///     `kClassD2cs` share value 0x01; the distinction comes from
///     which listener received the byte.)
constexpr InitConnResponse dispatch_init_conn(InitConnRequest req) noexcept
{
    using namespace pvpgn::protocol::bnet::init;
    // R168.b: per-IP rate limit. The legacy code applies this
    // BEFORE the class switch and exempts D2CS_BNETD; reproduce
    // that here so v3 owns the decision.
    if (req.max_conns_per_ip != 0 &&
        req.cclass != kClassD2csBnetd &&
        req.conn_count > req.max_conns_per_ip) {
        return {InitDecision::kRateLimited};
    }
    // R168.c: realmlist gate for D2CS_BNETD requests.
    if (req.cclass == kClassD2csBnetd && !req.d2cs_ip_allowed) {
        return {InitDecision::kD2csIpDenied};
    }
    switch (req.cclass) {
    case kClassBnet:      return {InitDecision::kBnet};
    case kClassFile:      return {InitDecision::kFile};
    case kClassBot:       return {InitDecision::kBot};
    case kClassTelnet:    return {InitDecision::kTelnet};
    case kClassD2csBnetd: return {InitDecision::kD2csBnetd};
    case kClassEnc:           // fallthrough -- legacy explicit reject
    case kClassLocalMachine:  // fallthrough -- legacy explicit reject
    default:
        return {InitDecision::kRejected};
    }
}

}  // namespace pvpgn::application::init
