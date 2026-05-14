// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file dispatch.hpp
/// Shared helper for the FINDANONGAME / BNet strangler bridges that
/// take an already-encoded `[FF 44 sz_lo sz_hi ...payload]` byte
/// sequence and inject it into the legacy outqueue as a `t_packet`.
///
/// Each bridge (clan_profile / profile / tournament / set_icon /
/// get_icon / anongame_inforeply) used to carry an in-file copy of
/// this routine. Centralising it here removes that duplication and
/// gives every future bridge a single hook to extend (logging, fault
/// injection, packet capture).
///
/// The header is intentionally free of legacy types: the function
/// takes `void* conn` so that callers do not have to wrap the
/// declaration in `setup_before.h` / `setup_after.h`. The
/// implementation in `dispatch.cpp` does the legacy include dance.

#include <cstddef>

namespace pvpgn::integration::legacy_bnetd {

/// Inject a fully-encoded BNet frame into the legacy outqueue.
///
/// Preconditions:
///   * `bytes[0]` == 0xFF (BNet marker)
///   * `bytes[1]` == 0x44 (FINDANONGAME / SID_WARCRAFTGENERAL)
///   * `bytes[2..3]` == size LE, equal to `size`
///   * `size` >= 4
///
/// Returns true on successful enqueue; false on any precondition
/// failure or if `packet_create` returns null.
bool dispatch_bnet_frame_v3(void* conn,
                            std::byte const* bytes,
                            std::size_t size);

}  // namespace pvpgn::integration::legacy_bnetd
