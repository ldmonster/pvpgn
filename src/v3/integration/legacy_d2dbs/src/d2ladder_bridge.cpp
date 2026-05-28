// SPDX-License-Identifier: GPL-2.0-or-later
//
// R231(2): observation bridge for d2dbs ladder load/save lifecycle.

#include "integration/legacy_d2dbs/d2ladder_bridge.hpp"

#include "core/logging.hpp"
#include "integration/legacy_d2dbs/bridge_logger.hpp"

namespace pld = pvpgn::integration::legacy_d2dbs;

extern "C" int pvpgn_v3_d2dbs_d2ladder_init_try(void) noexcept {
    pld::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2dbs_d2ladder_bridge",
        "d2ladder init observed",
        {});
    return 0;
}

extern "C" int pvpgn_v3_d2dbs_d2ladder_destroy_try(void) noexcept {
    pld::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2dbs_d2ladder_bridge",
        "d2ladder destroy observed",
        {});
    return 0;
}
