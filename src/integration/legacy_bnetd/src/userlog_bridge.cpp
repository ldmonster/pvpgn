// SPDX-License-Identifier: GPL-2.0-or-later
//
// R230(3): observation bridge for userlog (admin audit log)
// lifecycle and per-event append.

#include "integration/legacy_bnetd/userlog_bridge.hpp"

#include <string_view>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

namespace plb = pvpgn::integration::legacy_bnetd;

extern "C" int pvpgn_v3_userlog_init(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_userlog_bridge",
        "userlog init observed",
        {});
    return 0;
}

extern "C" int pvpgn_v3_userlog_append(char const* username,
                                            char const* text) noexcept {
    std::string_view user = (username != nullptr)
                                ? std::string_view{username}
                                : std::string_view{};
    std::string_view body = (text != nullptr)
                                ? std::string_view{text}
                                : std::string_view{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"user", user},
        {"text", body},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_userlog_bridge",
        "userlog append observed",
        {fields[0], fields[1]});
    return 0;
}
