// SPDX-License-Identifier: GPL-2.0-or-later
//
// Observation bridge for anongame_infos lifecycle. See header.

#include "integration/legacy_bnetd/anongame_infos_bridge.hpp"

#include <string_view>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

namespace plb = pvpgn::integration::legacy_bnetd;

extern "C" int pvpgn_v3_anongame_infos_load(char const* filename) noexcept {
    std::string_view name = (filename != nullptr)
                                ? std::string_view{filename}
                                : std::string_view{};

    const pvpgn::core::ILogger::Field fields[] = {
        {"file", name},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_anongame_infos_bridge",
        "anongame_infos load observed",
        {fields[0]});

    return 0;
}

extern "C" int pvpgn_v3_anongame_infos_unload(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_anongame_infos_bridge",
        "anongame_infos unload observed",
        {});

    return 0;
}
