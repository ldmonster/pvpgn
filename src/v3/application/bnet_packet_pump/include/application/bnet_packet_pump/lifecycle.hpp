// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file lifecycle.hpp
/// V3 packet-pump lifecycle FSM (R179.a scaffold).
///
/// The legacy bnet listener accepts a TCP connection in
/// `conn_class_init`, reads exactly one byte
/// (`CLIENT_INITCONN_CLASS_*`), then transitions the connection
/// to its real class (bnet / bot / telnet / file / d2cs_bnetd /
/// ...). After that, all subsequent packets are dispatched per
/// the class-specific handler.
///
/// This header captures that exact state machine as a pure
/// function. No I/O, no legacy types: feed it `(current_state,
/// cclass_byte)` and get back the next state. The packet pump
/// (future R179.c+) will drive this FSM directly off the bytes
/// returned by `protocol/bnet/init_codec.hpp`.

#include "application/bnet_packet_pump/conn_class.hpp"

#include "protocol/bnet/init_wire_types.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace pvpgn::application::bnet_packet_pump {

/// FSM states for a single v3 bnet connection.
enum class Lifecycle : std::uint8_t {
    kAwaitingInit = 0,  ///< Just-accepted; expects one cclass byte.
    kDispatching  = 1,  ///< Init byte received; per-class handler owns the connection.
    kRejected     = 2,  ///< Init byte was unrecognised; egress should close.
    kClosed       = 3,  ///< Terminal: socket closed / handler returned -1.
};

constexpr std::string_view to_string(Lifecycle s) noexcept {
    switch (s) {
        case Lifecycle::kAwaitingInit: return "awaiting_init";
        case Lifecycle::kDispatching:  return "dispatching";
        case Lifecycle::kRejected:     return "rejected";
        case Lifecycle::kClosed:       return "closed";
    }
    return "?";
}

/// Map a wire `cclass` byte to the matching `ConnClass`. Mirrors
/// the legacy switch in `handle_init_packet`. Unknown bytes
/// return `std::nullopt` -> caller transitions to `kRejected`.
///
/// Note: `CLIENT_INITCONN_CLASS_BNET` (0x01) and
/// `CLIENT_INITCONN_CLASS_D2CS` (0x01) share a wire value at
/// this layer; the legacy code maps 0x01 to `conn_class_bnet`
/// and decides D2CS-ness from the listener / address later. The
/// scaffold keeps that semantics.
constexpr std::optional<ConnClass>
conn_class_from_cclass_byte(std::uint8_t cclass) noexcept {
    namespace init = ::pvpgn::protocol::bnet::init;
    switch (cclass) {
        case init::kClassBnet:         return ConnClass::kBnet;
        case init::kClassFile:         return ConnClass::kFile;
        case init::kClassBot:          return ConnClass::kBot;
        case init::kClassEnc:          return ConnClass::kBnet;  ///< legacy treats Enc as a Bnet variant
        case init::kClassTelnet:       return ConnClass::kTelnet;
        case init::kClassD2gs:         return ConnClass::kD2cs;
        case init::kClassD2csBnetd:    return ConnClass::kD2csBnetd;
        case init::kClassLocalMachine: return ConnClass::kBnet;
        default:                       return std::nullopt;
    }
}

/// Outcome of feeding one byte into the FSM.
struct LifecycleStep {
    Lifecycle next_state = Lifecycle::kAwaitingInit;
    ConnClass class_now  = ConnClass::kNone;  ///< Valid only when next_state == kDispatching.

    constexpr bool operator==(const LifecycleStep&) const = default;
};

/// Drive the FSM with one cclass byte. Pure: no side effects.
/// Only `kAwaitingInit` accepts a byte; in all other states the
/// step is a no-op that preserves the current state.
constexpr LifecycleStep
step_on_cclass_byte(Lifecycle current, std::uint8_t cclass_byte) noexcept {
    if (current != Lifecycle::kAwaitingInit) {
        return LifecycleStep{ current, ConnClass::kNone };
    }
    auto const mapped = conn_class_from_cclass_byte(cclass_byte);
    if (!mapped.has_value()) {
        return LifecycleStep{ Lifecycle::kRejected, ConnClass::kNone };
    }
    return LifecycleStep{ Lifecycle::kDispatching, *mapped };
}

}  // namespace pvpgn::application::bnet_packet_pump
