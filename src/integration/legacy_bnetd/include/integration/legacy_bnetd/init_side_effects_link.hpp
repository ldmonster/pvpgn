// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file init_side_effects_link.hpp
/// R186.a: accessor for the linked-half implementation of the
/// `application::bnet_packet_pump::InitSideEffects` port. The
/// returned reference is to a `static` table with file-scope
/// lifetime, so callers can safely capture its address.

#include "application/bnet_packet_pump/init_side_effects.hpp"

namespace pvpgn::integration::legacy_bnetd {

/// Return the singleton `InitSideEffects` table whose entries
/// thunk to the legacy bnetd connection-state and `handle_d2cs_init`
/// functions. Thread-safe (one-time `static` init).
::pvpgn::application::bnet_packet_pump::InitSideEffects const&
get_legacy_init_side_effects() noexcept;

}  // namespace pvpgn::integration::legacy_bnetd
