// SPDX-License-Identifier: GPL-2.0-or-later
//
// Observation bridges for d2dbs signal init + per-tick
// dispatch.

#include "app/d2dbs/legacy_d2dbs_bridges/handle_signal_bridge.hpp"

#include "core/logging.hpp"
#include "app/d2dbs/legacy_d2dbs_bridges/bridge_logger.hpp"

namespace pld = pvpgn::integration::legacy_d2dbs;

extern "C" int pvpgn_v3_d2dbs_handle_signal_init(void) noexcept {
    pld::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2dbs_signal_bridge",
        "signal init observed",
        {});
    return 0;
}

extern "C" int pvpgn_v3_d2dbs_handle_signal(void) noexcept {
    pld::bridge_log_kv(pvpgn::core::LogLevel::Trace,
        "v3_d2dbs_signal_bridge",
        "signal dispatch observed",
        {});
    return 0;
}
