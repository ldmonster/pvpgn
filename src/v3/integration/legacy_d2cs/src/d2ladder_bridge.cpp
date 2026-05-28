// SPDX-License-Identifier: GPL-2.0-or-later
//
// R233(1): observation bridge for d2cs ladder load/destroy lifecycle.

#include "integration/legacy_d2cs/d2ladder_bridge.hpp"

#include "core/logging.hpp"
#include "integration/legacy_d2cs/bridge_logger.hpp"

namespace plc = pvpgn::integration::legacy_d2cs;

extern "C" int pvpgn_v3_d2cs_d2ladder_init_try(void) noexcept {
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_d2ladder_bridge",
        "d2ladder init observed",
        {});
    return 0;
}

extern "C" int pvpgn_v3_d2cs_d2ladder_destroy_try(void) noexcept {
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_d2ladder_bridge",
        "d2ladder destroy observed",
        {});
    return 0;
}
