// SPDX-License-Identifier: GPL-2.0-or-later
//
// Observation bridge for anongame search/join entry points.

#include "integration/legacy_bnetd/anongame_entry_bridge.hpp"

#include <string_view>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

namespace plb = pvpgn::integration::legacy_bnetd;

extern "C" int pvpgn_v3_anongame_entry_try(void* conn_ptr,
                                           char const* kind) noexcept {
    if (conn_ptr == nullptr) return 0;

    std::string_view kind_sv = (kind != nullptr)
                                   ? std::string_view{kind}
                                   : std::string_view{"?"};

    const pvpgn::core::ILogger::Field fields[] = {
        {"kind", kind_sv},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_anongame_entry_bridge",
        "anongame entry observed",
        {fields[0]});

    return 0;
}
