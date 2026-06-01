// SPDX-License-Identifier: GPL-2.0-or-later
//
// R232(3): observation bridges for d2dbs signal init + per-tick
// dispatch.

#include "integration/legacy_d2dbs/handle_signal_bridge.hpp"

#include "core/logging.hpp"
#include "integration/legacy_d2dbs/bridge_logger.hpp"

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
