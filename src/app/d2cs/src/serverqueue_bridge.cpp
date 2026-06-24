// SPDX-License-Identifier: GPL-2.0-or-later
//
// Observation bridges for d2cs server-queue lifecycle.

#include "app/d2cs/legacy_d2cs_bridges/serverqueue_bridge.hpp"

#include "core/logging.hpp"
#include "app/d2cs/legacy_d2cs_bridges/bridge_logger.hpp"

namespace plc = pvpgn::integration::legacy_d2cs;

extern "C" int pvpgn_v3_d2cs_sqlist_create(void) noexcept {
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_serverqueue_bridge",
        "sqlist create observed",
        {});
    return 0;
}

extern "C" int pvpgn_v3_d2cs_sqlist_destroy(void) noexcept {
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_serverqueue_bridge",
        "sqlist destroy observed",
        {});
    return 0;
}
