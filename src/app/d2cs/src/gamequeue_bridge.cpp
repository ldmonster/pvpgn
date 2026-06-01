// SPDX-License-Identifier: GPL-2.0-or-later
//
// R236(1): observation bridges for d2cs game-queue lifecycle.

#include "integration/legacy_d2cs/gamequeue_bridge.hpp"

#include "core/logging.hpp"
#include "integration/legacy_d2cs/bridge_logger.hpp"

namespace plc = pvpgn::integration::legacy_d2cs;

extern "C" int pvpgn_v3_d2cs_gqlist_create(void) noexcept {
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_gamequeue_bridge",
        "gqlist create observed",
        {});
    return 0;
}

extern "C" int pvpgn_v3_d2cs_gqlist_destroy(void) noexcept {
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_gamequeue_bridge",
        "gqlist destroy observed",
        {});
    return 0;
}
