// SPDX-License-Identifier: GPL-2.0-or-later
//
// R234(1): observation bridge for d2cs main event-loop entry.

#include "integration/legacy_d2cs/server_bridge.hpp"

#include "core/logging.hpp"
#include "integration/legacy_d2cs/bridge_logger.hpp"

namespace plc = pvpgn::integration::legacy_d2cs;

extern "C" int pvpgn_v3_d2cs_server_process(void) noexcept {
    plc::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_d2cs_server_bridge",
        "server_process entry observed",
        {});
    return 0;
}
