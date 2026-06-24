// SPDX-License-Identifier: GPL-2.0-or-later
//
// Observation bridge for d2cs ladder load/destroy lifecycle.

#include "app/d2cs/legacy_d2cs_bridges/d2ladder_bridge.hpp"

#include "core/logging.hpp"
#include "app/d2cs/legacy_d2cs_bridges/bridge_logger.hpp"

namespace plc = pvpgn::integration::legacy_d2cs;

extern "C" int pvpgn_v3_d2cs_d2ladder_init(void) noexcept {
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_d2ladder_bridge",
        "d2ladder init observed",
        {});
    return 0;
}

extern "C" int pvpgn_v3_d2cs_d2ladder_destroy(void) noexcept {
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_d2ladder_bridge",
        "d2ladder destroy observed",
        {});
    return 0;
}
