// SPDX-License-Identifier: GPL-2.0-or-later
//
// Placeholder TU so the `application_bnet_packet_pump` static lib
// has at least one object file under MSVC / Ninja. All actual
// scaffold lives in the headers (`conn_class.hpp`,
// `lifecycle.hpp`). Future rounds (R179.c+) add real driver code
// here.

namespace pvpgn::application::bnet_packet_pump {

// Sentinel to keep the TU non-empty.
inline constexpr int kScaffoldRound = 179;

}  // namespace pvpgn::application::bnet_packet_pump
