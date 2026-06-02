// SPDX-License-Identifier: GPL-2.0-or-later
//
// R238: observation bridges for d2cs game catalogue + per-game lifecycle.

#include "app/d2cs/legacy_d2cs_bridges/game_bridge.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <string_view>
#include <system_error>

#include "core/logging.hpp"
#include "app/d2cs/legacy_d2cs_bridges/bridge_logger.hpp"

namespace plc = pvpgn::integration::legacy_d2cs;

namespace {

std::string_view render_uint(std::array<char, 20>& buf,
                              unsigned int v) noexcept {
    auto res = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    if (res.ec != std::errc{}) return std::string_view{};
    return std::string_view{buf.data(),
        static_cast<std::size_t>(res.ptr - buf.data())};
}

std::string_view safe_str(const char* s) noexcept {
    return s ? std::string_view{s} : std::string_view{"<null>"};
}

}  // namespace

extern "C" int pvpgn_v3_d2cs_gamelist_create(void) noexcept {
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_game_bridge",
        "gamelist create observed",
        {});
    return 0;
}

extern "C" int pvpgn_v3_d2cs_gamelist_destroy(void) noexcept {
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_game_bridge",
        "gamelist destroy observed",
        {});
    return 0;
}

extern "C" int pvpgn_v3_d2cs_game_create(
    unsigned int id,
    const char* gamename,
    unsigned int gameflag) noexcept {
    std::array<char, 20> idbuf{};
    std::array<char, 20> flagbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"id",       render_uint(idbuf,   id)},
        {"gamename", safe_str(gamename)},
        {"gameflag", render_uint(flagbuf, gameflag)},
    };
    plc::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_d2cs_game_bridge",
        "game create observed",
        {fields[0], fields[1], fields[2]});
    return 0;
}

extern "C" int pvpgn_v3_d2cs_game_destroy(
    unsigned int id,
    const char* gamename) noexcept {
    std::array<char, 20> idbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"id",       render_uint(idbuf, id)},
        {"gamename", safe_str(gamename)},
    };
    plc::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_d2cs_game_bridge",
        "game destroy observed",
        {fields[0], fields[1]});
    return 0;
}

// R239(1): observation bridge for game_set_d2gs_gameid.
extern "C" int pvpgn_v3_d2cs_game_set_d2gs_gameid(
    unsigned int game_id,
    unsigned int d2gs_gameid) noexcept {
    std::array<char, 20> gidbuf{};
    std::array<char, 20> dgidbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"game_id",     render_uint(gidbuf,  game_id)},
        {"d2gs_gameid", render_uint(dgidbuf, d2gs_gameid)},
    };
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_game_bridge",
        "game set d2gs_gameid observed",
        {fields[0], fields[1]});
    return 0;
}

// R239(2): observation bridge for game_set_d2gs.
extern "C" int pvpgn_v3_d2cs_game_set_d2gs(
    unsigned int game_id,
    unsigned int d2gs_id) noexcept {
    std::array<char, 20> gidbuf{};
    std::array<char, 20> didbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"game_id", render_uint(gidbuf, game_id)},
        {"d2gs_id", render_uint(didbuf, d2gs_id)},
    };
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_game_bridge",
        "game set d2gs observed",
        {fields[0], fields[1]});
    return 0;
}

// R239(3): observation bridge for game_set_created.
extern "C" int pvpgn_v3_d2cs_game_set_created(
    unsigned int game_id,
    unsigned int created) noexcept {
    std::array<char, 20> gidbuf{};
    std::array<char, 20> cbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"game_id", render_uint(gidbuf, game_id)},
        {"created", render_uint(cbuf,   created)},
    };
    plc::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_d2cs_game_bridge",
        "game set created observed",
        {fields[0], fields[1]});
    return 0;
}

// R240(1): observation bridge for game_add_character.
extern "C" int pvpgn_v3_d2cs_game_add_character(
    unsigned int game_id,
    const char* charname,
    unsigned int chclass,
    unsigned int level) noexcept {
    std::array<char, 20> gidbuf{};
    std::array<char, 20> ccbuf{};
    std::array<char, 20> lvbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"game_id",  render_uint(gidbuf, game_id)},
        {"charname", safe_str(charname)},
        {"chclass",  render_uint(ccbuf,  chclass)},
        {"level",    render_uint(lvbuf,  level)},
    };
    plc::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_d2cs_game_bridge",
        "game add character observed",
        {fields[0], fields[1], fields[2], fields[3]});
    return 0;
}

// R240(2): observation bridge for game_del_character.
extern "C" int pvpgn_v3_d2cs_game_del_character(
    unsigned int game_id,
    const char* charname) noexcept {
    std::array<char, 20> gidbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"game_id",  render_uint(gidbuf, game_id)},
        {"charname", safe_str(charname)},
    };
    plc::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_d2cs_game_bridge",
        "game del character observed",
        {fields[0], fields[1]});
    return 0;
}
