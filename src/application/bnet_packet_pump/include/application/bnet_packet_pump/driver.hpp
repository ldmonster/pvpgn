// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file driver.hpp
/// V3 packet-pump driver class (R180.c).
///
/// Owns a `Lifecycle` per connection and exposes a single
/// `feed(span)` entry that mirrors what
/// `legacy_bnet_frame_router_link.cpp` will eventually do: in
/// `kAwaitingInit`, parse the one-byte init packet via
/// `protocol::bnet::init::parse_client_initconn`, drive the FSM,
/// and report the outcome to the caller.
///
/// Still pure-C++: no legacy types, no I/O, no `t_connection`,
/// no `t_packet`. The driver is constructible / movable, and
/// every method is `[[nodiscard]]`-friendly. Integration with
/// `LegacyBnetFrameRouter` lands in a future round (after R180.a
/// / R180.b clear the linked-adapter pivots).

#include "application/bnet_packet_pump/lifecycle.hpp"
#include "application/bnet_packet_pump/init_side_effects.hpp"
#include "protocol/bnet/init_codec.hpp"
#include "application/init/init_conn_dispatch.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace pvpgn::application::bnet_packet_pump {

/// Outcome the driver reports back to its caller after consuming
/// one frame.
enum class FeedOutcome : std::uint8_t {
    kAccepted     = 0,  ///< Frame processed; connection still alive.
    kRejected     = 1,  ///< Init byte was unrecognised; caller should close.
    kAlreadyOpen  = 2,  ///< Frame arrived after init handshake -- caller should hand off to per-class handler.
    kMalformed    = 3,  ///< Frame size didn't match the expected single byte.
    kClosed       = 4,  ///< Driver is in terminal kClosed state -- caller should drop.
    kRateLimited  = 5,  ///< R182.a: per-IP rate-limit exceeded; reject without opening.
    kD2csIpDenied = 6,  ///< R182.a: D2CS_BNETD client whose IP is not in the realmlist.
};

constexpr std::string_view to_string(FeedOutcome o) noexcept {
    switch (o) {
        case FeedOutcome::kAccepted:     return "accepted";
        case FeedOutcome::kRejected:     return "rejected";
        case FeedOutcome::kAlreadyOpen:  return "already_open";
        case FeedOutcome::kMalformed:    return "malformed";
        case FeedOutcome::kClosed:       return "closed";
        case FeedOutcome::kRateLimited:  return "rate_limited";
        case FeedOutcome::kD2csIpDenied: return "d2cs_ip_denied";
    }
    return "?";
}

/// R182.a: per-IP / realmlist policy the driver consults BEFORE
/// the cclass-byte FSM step. Mirrors the legacy gates inside
/// `pvpgn_v3_init_conn_apply_ex`.
///
/// A default-constructed `PumpPolicy` (or one with
/// `max_conns_per_ip == 0`) disables the rate-limit check and
/// keeps `d2cs_ip_allowed = true`, so callers that do not yet
/// know the policy can stay on the byte-only `feed()` overload.
struct PumpPolicy {
    unsigned int conn_count       = 0;
    unsigned int max_conns_per_ip = 0;
    bool         d2cs_ip_allowed  = true;
};

/// V3 packet-pump driver. One instance per connection.
class PacketPumpDriver {
public:
    [[nodiscard]] constexpr Lifecycle state()       const noexcept { return state_; }
    [[nodiscard]] constexpr ConnClass class_now()   const noexcept { return class_; }
    [[nodiscard]] constexpr bool      is_open()     const noexcept { return state_ == Lifecycle::kDispatching; }
    [[nodiscard]] constexpr bool      is_closed()   const noexcept { return state_ == Lifecycle::kClosed; }
    [[nodiscard]] constexpr bool      is_rejected() const noexcept { return state_ == Lifecycle::kRejected; }

    /// Feed one frame. Behaviour:
    /// - `kAwaitingInit`: frame MUST be exactly one byte; calls
    ///   `parse_client_initconn` and drives the FSM. Returns
    ///   `kAccepted` on known cclass, `kRejected` on unknown,
    ///   `kMalformed` on wrong size.
    /// - `kDispatching`: returns `kAlreadyOpen` -- the caller's
    ///   per-class handler owns the frame.
    /// - `kRejected` / `kClosed`: returns `kClosed`.
    constexpr FeedOutcome feed(std::span<const std::byte> frame) noexcept {
        return feed(frame, PumpPolicy{});
    }

    /// R182.a: policy-aware overload. Consults `application/init`'s
    /// `dispatch_init_conn` so per-IP rate limits and the D2CS_BNETD
    /// realmlist gate are applied BEFORE the cclass byte opens
    /// the connection. The byte-only `feed()` above calls this
    /// with a default `PumpPolicy{}` so its semantics are unchanged.
    constexpr FeedOutcome feed(std::span<const std::byte> frame,
                               PumpPolicy                 policy) noexcept {
        switch (state_) {
            case Lifecycle::kAwaitingInit: {
                auto const parsed = ::pvpgn::protocol::bnet::init::parse_client_initconn(frame);
                if (!parsed.has_value()) {
                    return FeedOutcome::kMalformed;
                }
                using namespace ::pvpgn::application::init;
                auto const verdict = dispatch_init_conn(InitConnRequest{
                    parsed->cclass,
                    policy.conn_count,
                    policy.max_conns_per_ip,
                    policy.d2cs_ip_allowed,
                });
                switch (verdict.decision) {
                    case InitDecision::kRateLimited:
                        state_ = Lifecycle::kRejected;
                        class_ = ConnClass::kNone;
                        return FeedOutcome::kRateLimited;
                    case InitDecision::kD2csIpDenied:
                        state_ = Lifecycle::kRejected;
                        class_ = ConnClass::kNone;
                        return FeedOutcome::kD2csIpDenied;
                    case InitDecision::kRejected:
                    case InitDecision::kBnet:
                    case InitDecision::kFile:
                    case InitDecision::kBot:
                    case InitDecision::kTelnet:
                    case InitDecision::kD2csBnetd:
                        break;
                }
                auto const step = step_on_cclass_byte(state_, parsed->cclass);
                state_ = step.next_state;
                class_ = step.class_now;
                return (state_ == Lifecycle::kDispatching)
                           ? FeedOutcome::kAccepted
                           : FeedOutcome::kRejected;
            }
            case Lifecycle::kDispatching:
                return FeedOutcome::kAlreadyOpen;
            case Lifecycle::kRejected:
            case Lifecycle::kClosed:
                return FeedOutcome::kClosed;
        }
        return FeedOutcome::kClosed;
    }

    /// Mark the connection as closed (egress saw EOF / handler
    /// returned -1). Idempotent.
    constexpr void close() noexcept {
        state_ = Lifecycle::kClosed;
        class_ = ConnClass::kNone;
    }

    /// R186.a: policy-aware feed that ALSO invokes the
    /// `InitSideEffects` callback table on accept. This is the
    /// entry the linked half uses now that `apply_via_legacy` is
    /// being retired: the driver consumes the cclass byte,
    /// consults `dispatch_init_conn` against `policy`, and on
    /// accept runs `set_connected` -> `set_class` -> (if
    /// `kD2csBnetd`) `apply_d2cs_init`. If `apply_d2cs_init`
    /// fails (returns -1), the driver flips its state to
    /// `kRejected` and reports `FeedOutcome::kRejected` to the
    /// caller -- the connection is dead. NULL entries in `sx`
    /// are treated as no-ops.
    FeedOutcome feed_with_side_effects(std::span<const std::byte> frame,
                                       PumpPolicy                 policy,
                                       void*                      conn,
                                       InitSideEffects const&     sx) noexcept {
        auto const outcome = feed(frame, policy);
        if (outcome != FeedOutcome::kAccepted) return outcome;

        // Driver accepted; run the side effects in legacy order:
        // log -> set_connected -> set_class -> (d2cs init).
        if (sx.log_accept    != nullptr) sx.log_accept(conn, class_);
        if (sx.set_connected != nullptr) sx.set_connected(conn);
        if (sx.set_class     != nullptr) sx.set_class(conn, class_);

        if (class_ == ConnClass::kD2csBnetd && sx.apply_d2cs_init != nullptr) {
            int const rc = sx.apply_d2cs_init(conn);
            if (rc < 0) {
                // D2CS handshake failed mid-accept: connection is dead.
                state_ = Lifecycle::kRejected;
                class_ = ConnClass::kNone;
                return FeedOutcome::kRejected;
            }
        }
        return FeedOutcome::kAccepted;
    }

private:
    Lifecycle state_ = Lifecycle::kAwaitingInit;
    ConnClass class_ = ConnClass::kNone;
};

}  // namespace pvpgn::application::bnet_packet_pump
