// SPDX-License-Identifier: GPL-2.0-or-later
//
// Observation bridge for SID_GAME_REPORT (0x40). See header.

#include "integration/legacy_bnetd/game_report_bridge.hpp"

#include <string>
#include <string_view>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

namespace plb = pvpgn::integration::legacy_bnetd;

extern "C" int pvpgn_v3_gamereport_try(void* conn_ptr,
                                       char const* username,
                                       unsigned int player_count) noexcept {
    if (conn_ptr == nullptr) return 0;

    std::string_view user = (username != nullptr)
                                ? std::string_view{username}
                                : std::string_view{};

    const std::string count_str = std::to_string(player_count);
    const pvpgn::core::ILogger::Field fields[] = {
        {"user",         user},
        {"player_count", std::string_view{count_str}},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_game_report_bridge",
        "gamereport intent observed",
        {fields[0], fields[1]});

    return 0;
}
