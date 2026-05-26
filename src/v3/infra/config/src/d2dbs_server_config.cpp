// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/config/d2dbs_server_config.hpp"

/// @file d2dbs_server_config.cpp
/// Implementation of `parse_d2dbs_server_config` / `load_d2dbs_server_config`.
///
/// R152 skeleton: section-by-section TOML parser modelled on
/// `server_config.cpp`. Defaults match `conf/d2dbs.toml.in`.

#include "infra/config/config.hpp"

#include <fstream>

namespace pvpgn::infra::config {

namespace {

auto u32 = [](std::int64_t v) { return static_cast<std::uint32_t>(v); };

void parse_network(const Config& cfg, D2dbsServerConfig& sc)
{
    if (auto sec = cfg.section("network")) {
        sc.network.servaddrs    = sec->get_or<std::string>("servaddrs",    sc.network.servaddrs);
        sc.network.gameservlist = sec->get_or<std::string>("gameservlist", sc.network.gameservlist);
    }
}

void parse_log(const Config& cfg, D2dbsServerConfig& sc)
{
    if (auto sec = cfg.section("log")) {
        sc.log.levels = sec->get_or<std::string>("levels", sc.log.levels);
    }
}

void parse_files(const Config& cfg, D2dbsServerConfig& sc)
{
    auto set_path = [](std::filesystem::path& dest, const Config& sec,
                       std::string_view key) {
        if (auto v = sec.get<std::string>(key))
            dest = *v;
    };
    if (auto sec = cfg.section("files")) {
        set_path(sc.files.logfile,          *sec, "logfile");
        set_path(sc.files.logfile_gs,       *sec, "logfile_gs");
        set_path(sc.files.charsave_dir,     *sec, "charsavedir");
        set_path(sc.files.charinfo_dir,     *sec, "charinfodir");
        set_path(sc.files.ladder_dir,       *sec, "ladderdir");
        set_path(sc.files.bak_charsave_dir, *sec, "bak_charsavedir");
        set_path(sc.files.bak_charinfo_dir, *sec, "bak_charinfodir");
        set_path(sc.files.pidfile,          *sec, "pidfile");
    }
}

void parse_ladder(const Config& cfg, D2dbsServerConfig& sc)
{
    if (auto sec = cfg.section("ladder")) {
        sc.ladder.laddersave_interval    = u32(sec->get_or<std::int64_t>("laddersave_interval",    sc.ladder.laddersave_interval));
        sc.ladder.ladderinit_time        = u32(sec->get_or<std::int64_t>("ladderinit_time",        sc.ladder.ladderinit_time));
        sc.ladder.XML_ladder_output      = sec->get_or<bool>("XML_ladder_output",                  sc.ladder.XML_ladder_output);
        sc.ladder.ladder_chars_only      = sec->get_or<bool>("ladder_chars_only",                  sc.ladder.ladder_chars_only);
        sc.ladder.ladderupdate_threshold = u32(sec->get_or<std::int64_t>("ladderupdate_threshold", sc.ladder.ladderupdate_threshold));
    }
}

void parse_misc(const Config& cfg, D2dbsServerConfig& sc)
{
    if (auto sec = cfg.section("misc")) {
        sc.misc.shutdown_delay        = u32(sec->get_or<std::int64_t>("shutdown_delay",        sc.misc.shutdown_delay));
        sc.misc.shutdown_decr         = u32(sec->get_or<std::int64_t>("shutdown_decr",         sc.misc.shutdown_decr));
        sc.misc.idletime              = u32(sec->get_or<std::int64_t>("idletime",              sc.misc.idletime));
        sc.misc.keepalive_interval    = u32(sec->get_or<std::int64_t>("keepalive_interval",    sc.misc.keepalive_interval));
        sc.misc.timeout_checkinterval = u32(sec->get_or<std::int64_t>("timeout_checkinterval", sc.misc.timeout_checkinterval));
        sc.misc.difficulty_hack       = u32(sec->get_or<std::int64_t>("difficulty_hack",       sc.misc.difficulty_hack));
    }
}

D2dbsServerConfig from_config(const Config& cfg)
{
    D2dbsServerConfig sc;
    parse_network(cfg, sc);
    parse_log(cfg, sc);
    parse_files(cfg, sc);
    parse_ladder(cfg, sc);
    parse_misc(cfg, sc);
    return sc;
}

}  // namespace

core::Result<D2dbsServerConfig, core::Error>
parse_d2dbs_server_config(std::string_view toml_text)
{
    auto cfg = Config::load_string(toml_text);
    if (!cfg) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "TOML parse error in d2dbs config"});
    }
    return from_config(*cfg);
}

core::Result<D2dbsServerConfig, core::Error>
load_d2dbs_server_config(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return core::fail(core::Error{
            core::StatusCode::NotFound,
            "cannot open d2dbs config file: " + path.string()});
    }
    auto cfg = Config::load_file(path.string());
    if (!cfg) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "TOML parse error in d2dbs config file: " + path.string()});
    }
    return from_config(*cfg);
}

}  // namespace pvpgn::infra::config
