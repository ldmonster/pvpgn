// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_udptest_bridge.hpp"

// SERVER_UDPTEST is sent via raw UDP (psock_sendto), not conn_push_outqueue.
// The bridge is therefore observation-only: it always returns 0, causing the
// legacy UDP send path to run unchanged.
//
// When the v3 UDP test path is implemented, this function can be upgraded to
// encode and send the packet via the v3 UDP socket abstraction.

extern "C" int pvpgn_v3_observe_udptest(void* /*conn_ptr*/) noexcept {
    return 0;
}
