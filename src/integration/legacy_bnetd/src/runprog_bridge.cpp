// SPDX-License-Identifier: GPL-2.0-or-later
//
// R230(1): observation bridge for runprog open/close lifecycle.

#include "integration/legacy_bnetd/runprog_bridge.hpp"

#include <string_view>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

namespace plb = pvpgn::integration::legacy_bnetd;

extern "C" int pvpgn_v3_runprog_open_try(char const* command) noexcept {
    std::string_view cmd = (command != nullptr)
                               ? std::string_view{command}
                               : std::string_view{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"command", cmd},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_runprog_bridge",
        "runprog open observed",
        {fields[0]});
    return 0;
}

extern "C" int pvpgn_v3_runprog_close_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_runprog_bridge",
        "runprog close observed",
        {});
    return 0;
}
