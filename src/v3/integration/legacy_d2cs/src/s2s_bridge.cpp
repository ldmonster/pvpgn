// SPDX-License-Identifier: GPL-2.0-or-later
//
// R233(2): observation bridge for d2cs server-to-server bootstrap.

#include "integration/legacy_d2cs/s2s_bridge.hpp"

#include "core/logging.hpp"
#include "integration/legacy_d2cs/bridge_logger.hpp"

namespace plc = pvpgn::integration::legacy_d2cs;

extern "C" int pvpgn_v3_d2cs_s2s_init_try(void) noexcept {
    plc::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_d2cs_s2s_bridge",
        "s2s init observed",
        {});
    return 0;
}
