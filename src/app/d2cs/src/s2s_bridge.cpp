// SPDX-License-Identifier: GPL-2.0-or-later
//
// Observation bridge for d2cs server-to-server bootstrap.

#include "app/d2cs/legacy_d2cs_bridges/s2s_bridge.hpp"

#include "core/logging.hpp"
#include "app/d2cs/legacy_d2cs_bridges/bridge_logger.hpp"

namespace plc = pvpgn::integration::legacy_d2cs;

extern "C" int pvpgn_v3_d2cs_s2s_init(void) noexcept {
    plc::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_d2cs_s2s_bridge",
        "s2s init observed",
        {});
    return 0;
}
