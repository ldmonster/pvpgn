// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_file_bridge.hpp"

// Both file_send() packet_create() sites involve binary file-transfer protocol
// structures (packet_class_file header + packet_class_raw body chunks).
// The v3 file transfer encoder is not yet implemented, so both bridges are
// observation-only: they always return 0, causing the legacy send path to run
// unchanged.
//
// When the v3 file transfer path is implemented, these functions can be
// upgraded to encode and push the packets via the v3 send_packet handler.

extern "C" int pvpgn_v3_observe_file_send(void* /*conn_ptr*/,
                                            void const* /*packet_ptr*/) noexcept {
    return 0;
}

extern "C" int pvpgn_v3_observe_file_raw_send(void* /*conn_ptr*/,
                                               void const* /*packet_ptr*/) noexcept {
    return 0;
}
