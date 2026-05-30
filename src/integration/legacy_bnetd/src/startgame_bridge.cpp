// SPDX-License-Identifier: GPL-2.0-or-later
//
// Observation bridge for SID_STARTGAME{1,3,4}. See header.

#include "integration/legacy_bnetd/startgame_bridge.hpp"

#include <cstdio>
#include <string>
#include <string_view>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

namespace plb = pvpgn::integration::legacy_bnetd;

namespace {

const char* version_name(unsigned int v) noexcept {
    switch (v) {
        case 1: return "STARTGAME1";
        case 3: return "STARTGAME3";
        case 4: return "STARTGAME4";
        default: return "?";
    }
}

std::string to_hex(unsigned int v) {
    char buf[2 + 8 + 1];
    std::snprintf(buf, sizeof(buf), "0x%08x", v);
    return std::string{buf};
}

}  // namespace

extern "C" int pvpgn_v3_startgame(void* conn_ptr,
                                      unsigned int version,
                                      char const* gamename,
                                      char const* gameinfo,
                                      unsigned int bngtype,
                                      unsigned int status,
                                      unsigned int flag,
                                      unsigned int option) noexcept {
    if (conn_ptr == nullptr) return 0;

    std::string_view name = (gamename != nullptr)
                                ? std::string_view{gamename}
                                : std::string_view{};
    std::string_view info = (gameinfo != nullptr)
                                ? std::string_view{gameinfo}
                                : std::string_view{};

    const std::string version_str = std::to_string(version);
    const std::string bngtype_str = to_hex(bngtype);
    const std::string status_str  = to_hex(status);
    const std::string flag_str    = to_hex(flag);
    const std::string option_str  = to_hex(option);
    const auto info_len = std::to_string(info.size());

    const pvpgn::core::ILogger::Field fields[] = {
        {"variant", std::string_view{version_name(version)}},
        {"version", std::string_view{version_str}},
        {"game",    name},
        {"bngtype", std::string_view{bngtype_str}},
        {"status",  std::string_view{status_str}},
        {"flag",    std::string_view{flag_str}},
        {"option",  std::string_view{option_str}},
        {"infolen", std::string_view{info_len}},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_startgame_bridge",
        "startgame intent observed",
        {fields[0], fields[1], fields[2], fields[3],
         fields[4], fields[5], fields[6], fields[7]});

    return 0;
}
