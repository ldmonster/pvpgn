// SPDX-License-Identifier: GPL-2.0-or-later
//
// R233(3): observation bridges for d2cs signal init + per-tick dispatch.

#include "app/d2cs/legacy_d2cs_bridges/handle_signal_bridge.hpp"

#include "core/logging.hpp"
#include "app/d2cs/legacy_d2cs_bridges/bridge_logger.hpp"

namespace plc = pvpgn::integration::legacy_d2cs;

extern "C" int pvpgn_v3_d2cs_handle_signal_init(void) noexcept {
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_signal_bridge",
        "signal init observed",
        {});
    return 0;
}

extern "C" int pvpgn_v3_d2cs_handle_signal(void) noexcept {
    plc::bridge_log_kv(pvpgn::core::LogLevel::Trace,
        "v3_d2cs_signal_bridge",
        "signal dispatch observed",
        {});
    return 0;
}
