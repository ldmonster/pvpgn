// SPDX-License-Identifier: GPL-2.0-or-later
//
// R236(3): observation bridges for d2cs d2gs-list lifecycle.

#include "integration/legacy_d2cs/d2gs_bridge.hpp"

#include <string_view>

#include "core/logging.hpp"
#include "integration/legacy_d2cs/bridge_logger.hpp"

namespace plc = pvpgn::integration::legacy_d2cs;

extern "C" int pvpgn_v3_d2cs_d2gslist_create(void) noexcept {
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_d2gs_bridge",
        "d2gslist create observed",
        {});
    return 0;
}

extern "C" int pvpgn_v3_d2cs_d2gslist_destroy(void) noexcept {
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_d2gs_bridge",
        "d2gslist destroy observed",
        {});
    return 0;
}

extern "C" int pvpgn_v3_d2cs_d2gslist_reload(const char* gslist) noexcept {
    // The reload bridge is observation-only: do NOT parse `gslist`.
    // Log the pointer's null-state plus, when non-null, the raw
    // string view -- it is owned by the caller (prefs subsystem) and
    // remains valid until the legacy `d2gslist_reload` body returns,
    // which strictly outlives the bridge call.
    const std::string_view gslist_view = (gslist != nullptr)
        ? std::string_view{gslist}
        : std::string_view{"<null>"};
    const pvpgn::core::ILogger::Field fields[] = {
        {"gslist", gslist_view},
    };
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_d2gs_bridge",
        "d2gslist reload observed",
        {fields[0]});
    return 0;
}
