// SPDX-License-Identifier: GPL-2.0-or-later
//
// Observation bridge for SID_CLAN_* handlers. See header.

#include "integration/legacy_bnetd/clan_dispatch_bridge.hpp"

#include <string_view>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

namespace plb = pvpgn::integration::legacy_bnetd;

extern "C" int pvpgn_v3_clan_dispatch_try(void* conn_ptr,
                                          char const* op) noexcept {
    if (conn_ptr == nullptr) {
        return 0;
    }

    std::string_view op_sv = (op != nullptr)
                                 ? std::string_view{op}
                                 : std::string_view{"?"};
    if (op_sv.empty()) op_sv = std::string_view{"?"};

    const pvpgn::core::ILogger::Field fields[] = {
        {"op", op_sv},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_clan_dispatch_bridge",
        "SID_CLAN_* dispatch observed",
        {fields[0]});

    return 0;
}
