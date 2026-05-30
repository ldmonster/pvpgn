// SPDX-License-Identifier: GPL-2.0-or-later
//
// Observation bridges for SID_GAMELISTREQ and SID_JOIN_GAME. See
// header for scope.

#include "integration/legacy_bnetd/gamelist_join_bridge.hpp"

#include <cstdio>
#include <string>
#include <string_view>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

namespace plb = pvpgn::integration::legacy_bnetd;

namespace {

std::string to_hex32(unsigned int v) {
    char buf[2 + 8 + 1];
    std::snprintf(buf, sizeof(buf), "0x%08x", v);
    return std::string{buf};
}

}  // namespace

extern "C" int pvpgn_v3_gamelistreq(void* conn_ptr,
                                        char const* gamename,
                                        unsigned int bngtype) noexcept {
    if (conn_ptr == nullptr) return 0;

    std::string_view name = (gamename != nullptr)
                                ? std::string_view{gamename}
                                : std::string_view{};
    const bool is_specific = !name.empty();

    const std::string bngtype_str = to_hex32(bngtype);
    const std::string_view scope_str = is_specific
        ? std::string_view{"specific"}
        : std::string_view{"public_list"};

    const pvpgn::core::ILogger::Field fields[] = {
        {"scope",   scope_str},
        {"game",    name},
        {"bngtype", std::string_view{bngtype_str}},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_gamelist_join_bridge",
        "gamelistreq intent observed",
        {fields[0], fields[1], fields[2]});

    return 0;
}

extern "C" int pvpgn_v3_joingame(void* conn_ptr,
                                     char const* gamename) noexcept {
    if (conn_ptr == nullptr) return 0;

    std::string_view name = (gamename != nullptr)
                                ? std::string_view{gamename}
                                : std::string_view{};

    const pvpgn::core::ILogger::Field fields[] = {
        {"game", name},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_gamelist_join_bridge",
        "joingame intent observed",
        {fields[0]});

    return 0;
}
