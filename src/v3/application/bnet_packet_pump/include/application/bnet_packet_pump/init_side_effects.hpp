// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file init_side_effects.hpp
/// R186.a: port (in the hexagonal sense) for the side effects the
/// init-byte handshake performs on the host connection object.
///
/// The pure-C++ v3 `PacketPumpDriver` (R180.c / R182.a) does not
/// know about `t_connection`, `conn_set_state`, `conn_set_class`
/// or `handle_d2cs_init` -- those are legacy-side concerns. To
/// keep the driver layer-clean while still letting it own the
/// authoritative accept path, the driver invokes an
/// `InitSideEffects` callback table on every accept. The linked
/// half (`integration_legacy_bnetd_linked`) provides a concrete
/// table that thunks each entry to the corresponding legacy
/// function (see `init_side_effects_link.cpp`). Tests can plug a
/// stub table.
///
/// Design notes:
/// - Function-pointer table (not pure-virtual class) so the
///   driver can stay `constexpr`-friendly and we avoid an
///   inheritance dependency edge from `application/` to legacy
///   types.
/// - Each callback takes the opaque `void* conn` the caller
///   passes in -- the driver itself never dereferences it; only
///   the table's entries do, and only the linked half knows the
///   concrete `t_connection*` type.
/// - `apply_d2cs_init` returns 0 on success, -1 on failure
///   (matches `handle_d2cs_init` semantics). All other callbacks
///   are void: they cannot fail in any way the driver needs to
///   react to.

#include "application/bnet_packet_pump/conn_class.hpp"

namespace pvpgn::application::bnet_packet_pump {

/// Callback table passed to the policy-aware feed() overload that
/// performs side effects on the host connection. A NULL field is
/// treated as "no-op" so callers can plug in partial
/// implementations during the cutover arc.
struct InitSideEffects {
    /// Mark the connection as `connected` (legacy `conn_set_state(c, conn_state_connected)`).
    void (*set_connected)(void* conn) noexcept = nullptr;

    /// Set the legacy connection class for the accepted cclass.
    /// `cclass` is the v3 `ConnClass` enumerator the driver
    /// decided on; the implementation maps it to the legacy
    /// `conn_class_*` value.
    void (*set_class)(void* conn, ConnClass cclass) noexcept = nullptr;

    /// Run the D2CS bnetd-init callout (legacy `handle_d2cs_init`).
    /// Returns 0 on success, -1 on failure. Only invoked for
    /// `ConnClass::kD2csBnetd`; for any other class the driver
    /// does not touch this entry.
    int (*apply_d2cs_init)(void* conn) noexcept = nullptr;

    /// Emit one info-level log line that the legacy switch used
    /// to print ("[fd] client initiated <kind> connection"). The
    /// linked half formats the connection's fd and a class-
    /// specific human string; the driver only signals "I just
    /// accepted this class". May be null -- the driver does not
    /// require it.
    void (*log_accept)(void* conn, ConnClass cclass) noexcept = nullptr;
};

}  // namespace pvpgn::application::bnet_packet_pump
