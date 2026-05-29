// SPDX-License-Identifier: GPL-2.0-or-later
//
// R236(2): observation bridges for d2cs server-queue lifecycle.

#include "integration/legacy_d2cs/serverqueue_bridge.hpp"

#include "core/logging.hpp"
#include "integration/legacy_d2cs/bridge_logger.hpp"

namespace plc = pvpgn::integration::legacy_d2cs;

extern "C" int pvpgn_v3_d2cs_sqlist_create_try(void) noexcept {
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_serverqueue_bridge",
        "sqlist create observed",
        {});
    return 0;
}

extern "C" int pvpgn_v3_d2cs_sqlist_destroy_try(void) noexcept {
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_serverqueue_bridge",
        "sqlist destroy observed",
        {});
    return 0;
}
