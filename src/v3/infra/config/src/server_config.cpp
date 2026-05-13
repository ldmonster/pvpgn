// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/config/server_config.hpp"

#include <toml++/toml.hpp>

#include <fstream>
#include <sstream>
#include <string>

namespace pvpgn::infra::config {

namespace {

core::LogLevel parse_log_level(std::string_view s, core::LogLevel fallback) {
    if (s == "trace")    return core::LogLevel::Trace;
    if (s == "debug")    return core::LogLevel::Debug;
    if (s == "info")     return core::LogLevel::Info;
    if (s == "warn")     return core::LogLevel::Warn;
    if (s == "warning")  return core::LogLevel::Warn;
    if (s == "error")    return core::LogLevel::Error;
    if (s == "critical") return core::LogLevel::Critical;
    if (s == "off")      return core::LogLevel::Off;
    return fallback;
}

ServerConfig from_table(const toml::table& root) {
    ServerConfig cfg;

    cfg.servername = root["server"]["name"].value_or(cfg.servername);
    if (auto sd = root["server"]["script_dir"].value<std::string>()) {
        cfg.script_dir = *sd;
    }

    cfg.network.bind_addr =
        root["network"]["bind_addr"].value_or(cfg.network.bind_addr);
    {
        const std::uint16_t default_port = cfg.network.port;
        cfg.network.port = static_cast<std::uint16_t>(
            root["network"]["port"].value_or<std::uint16_t>(
                std::uint16_t{default_port}));
    }

    if (auto lvl = root["log"]["level"].value<std::string>()) {
        cfg.log.level = parse_log_level(*lvl, cfg.log.level);
    }
    if (auto f = root["log"]["file"].value<std::string>()) {
        cfg.log.file = *f;
    }
    cfg.log.rotate_size =
        root["log"]["rotate_size"].value_or<std::size_t>(std::size_t{cfg.log.rotate_size});
    cfg.log.rotate_files =
        root["log"]["rotate_files"].value_or<std::size_t>(std::size_t{cfg.log.rotate_files});
    cfg.log.stdout_sink =
        root["log"]["stdout"].value_or(cfg.log.stdout_sink);

    cfg.storage.driver =
        root["storage"]["driver"].value_or(cfg.storage.driver);
    cfg.storage.dsn = root["storage"]["dsn"].value_or(cfg.storage.dsn);
    cfg.storage.pool =
        root["storage"]["pool"].value_or<std::uint32_t>(std::uint32_t{cfg.storage.pool});

    return cfg;
}

}  // namespace

core::Result<ServerConfig, core::Error>
parse_server_config(std::string_view toml_text) {
    try {
        auto tbl = toml::parse(toml_text);
        return from_table(tbl);
    } catch (const toml::parse_error& e) {
        std::ostringstream os;
        os << "TOML parse error: " << e.description();
        if (auto sr = e.source().begin; sr.line != 0) {
            os << " (line " << sr.line << ", col " << sr.column << ")";
        }
        return core::fail(
            core::Error{core::StatusCode::InvalidArgument, os.str()});
    } catch (const std::exception& e) {
        return core::fail(
            core::Error{core::StatusCode::Internal, e.what()});
    }
}

core::Result<ServerConfig, core::Error>
load_server_config(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return core::fail(core::Error{
            core::StatusCode::NotFound,
            "cannot open config file: " + path.string()});
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    return parse_server_config(buf.str());
}

}  // namespace pvpgn::infra::config
