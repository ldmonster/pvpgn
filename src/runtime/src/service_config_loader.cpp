// SPDX-License-Identifier: GPL-2.0-or-later
#include "runtime/service_config_loader.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <vector>

namespace pvpgn::runtime {

// Simple TOML parser (minimal implementation for basic key-value pairs)
class TomlParser {
public:
    static Result<std::unordered_map<std::string, std::string>, Error> parse(std::string_view content)
    {
        std::unordered_map<std::string, std::string> result;
        std::string content_str(content);
        std::istringstream iss(content_str);
        std::string line;
        std::string current_section;
        
        while (std::getline(iss, line)) {
            // Trim whitespace
            line = trim(line);
            
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#' || line[0] == ';') {
                continue;
            }
            
            // Handle sections [section]
            if (line[0] == '[' && line[line.length() - 1] == ']') {
                current_section = line.substr(1, line.length() - 2);
                current_section = trim(current_section);
                continue;
            }
            
            // Parse key = value
            size_t eq_pos = line.find('=');
            if (eq_pos == std::string::npos) {
                continue;
            }
            
            std::string key = trim(line.substr(0, eq_pos));
            std::string value = trim(line.substr(eq_pos + 1));
            
            // Remove quotes if present
            if ((value.front() == '"' && value.back() == '"') ||
                (value.front() == '\'' && value.back() == '\'')) {
                value = value.substr(1, value.length() - 2);
            }
            
            // Build full key with section prefix
            std::string full_key = key;
            if (!current_section.empty()) {
                full_key = current_section + "." + key;
            }
            
            result[full_key] = value;
        }
        
        return Result<std::unordered_map<std::string, std::string>, Error>(result);
    }

private:
    static std::string trim(const std::string& str)
    {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return "";
        }
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, last - first + 1);
    }
};

Result<ServiceRuntimeConfig, Error> ServiceConfigLoader::load_toml(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path)) {
        return Result<ServiceRuntimeConfig, Error>(
            core::fail(core::make_error(core::StatusCode::NotFound, 
                "Configuration file not found: " + path.string()))
        );
    }
    
    std::ifstream file(path);
    if (!file.is_open()) {
        return Result<ServiceRuntimeConfig, Error>(
            core::fail(core::make_error(core::StatusCode::PermissionDenied,
                "Failed to open configuration file: " + path.string()))
        );
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    return load_toml_string(buffer.str());
}

Result<ServiceRuntimeConfig, Error> ServiceConfigLoader::load_toml_string(std::string_view content)
{
    auto parse_result = TomlParser::parse(content);
    if (!parse_result) {
        return Result<ServiceRuntimeConfig, Error>(core::fail(parse_result.error()));
    }
    
    auto& config_map = parse_result.value();
    ServiceRuntimeConfig config;
    
    // Extract service name
    auto it = config_map.find("service.name");
    if (it != config_map.end()) {
        config.service_name = it->second;
    } else {
        return Result<ServiceRuntimeConfig, Error>(
            core::fail(core::make_error(core::StatusCode::InvalidArgument,
                "Missing required field: service.name"))
        );
    }
    
    // Extract logging configuration
    it = config_map.find("logging.level");
    if (it != config_map.end()) {
        config.log_level = it->second;
    }
    
    it = config_map.find("logging.file");
    if (it != config_map.end()) {
        config.log_file = it->second;
    }
    
    // Extract daemon configuration
    it = config_map.find("daemon.enabled");
    if (it != config_map.end()) {
        config.daemon = (it->second == "true" || it->second == "1" || it->second == "yes");
    }
    
    it = config_map.find("daemon.pid_file");
    if (it != config_map.end()) {
        config.pid_file = it->second;
    }
    
    it = config_map.find("daemon.run_as_user");
    if (it != config_map.end()) {
        config.run_as_user = it->second;
    }
    
    // Extract network configuration
    it = config_map.find("network.bind_address");
    if (it != config_map.end()) {
        config.network.bind_address = it->second;
    }
    
    it = config_map.find("network.port");
    if (it != config_map.end()) {
        try {
            config.network.port = static_cast<uint16_t>(std::stoul(it->second));
        } catch (...) {
            return Result<ServiceRuntimeConfig, Error>(
                core::fail(core::make_error(core::StatusCode::InvalidArgument,
                    "Invalid port number: " + it->second))
            );
        }
    }
    
    it = config_map.find("network.tls_enabled");
    if (it != config_map.end()) {
        config.network.tls_enabled = (it->second == "true" || it->second == "1" || it->second == "yes");
    }
    
    it = config_map.find("network.cert_file");
    if (it != config_map.end()) {
        config.network.cert_file = it->second;
    }
    
    it = config_map.find("network.key_file");
    if (it != config_map.end()) {
        config.network.key_file = it->second;
    }
    
    it = config_map.find("network.ca_file");
    if (it != config_map.end()) {
        config.network.ca_file = it->second;
    }
    
    // Store extra configuration
    for (const auto& [key, value] : config_map) {
        // Skip known keys
        if (key.find("service.") == 0 || key.find("logging.") == 0 ||
            key.find("daemon.") == 0 || key.find("network.") == 0) {
            continue;
        }
        config.extra[key] = value;
    }
    
    // Validate configuration
    auto validate_result = validate(config);
    if (!validate_result) {
        return Result<ServiceRuntimeConfig, Error>(core::fail(validate_result.error()));
    }
    
    return Result<ServiceRuntimeConfig, Error>(config);
}

Result<void, Error> ServiceConfigLoader::validate(const ServiceRuntimeConfig& cfg)
{
    // Validate service name
    if (cfg.service_name.empty()) {
        return Result<void, Error>(
            core::fail(core::make_error(core::StatusCode::InvalidArgument,
                "Service name cannot be empty"))
        );
    }
    
    // Validate log level
    const std::vector<std::string> valid_levels = {
        "trace", "debug", "info", "warn", "error", "critical"
    };
    
    std::string log_level_lower = cfg.log_level;
    std::transform(log_level_lower.begin(), log_level_lower.end(),
                   log_level_lower.begin(), ::tolower);
    
    if (std::find(valid_levels.begin(), valid_levels.end(), log_level_lower) == valid_levels.end()) {
        return Result<void, Error>(
            core::fail(core::make_error(core::StatusCode::InvalidArgument,
                "Invalid log level: " + cfg.log_level))
        );
    }
    
    // Validate network configuration
    if (cfg.network.port == 0) {
        return Result<void, Error>(
            core::fail(core::make_error(core::StatusCode::InvalidArgument,
                "Network port must be specified and non-zero"))
        );
    }
    
    // Validate TLS configuration if enabled
    if (cfg.network.tls_enabled) {
        if (cfg.network.cert_file.empty()) {
            return Result<void, Error>(
                core::fail(core::make_error(core::StatusCode::InvalidArgument,
                    "TLS enabled but cert_file not specified"))
            );
        }
        if (cfg.network.key_file.empty()) {
            return Result<void, Error>(
                core::fail(core::make_error(core::StatusCode::InvalidArgument,
                    "TLS enabled but key_file not specified"))
            );
        }
    }
    
    return Result<void, Error>();
}

} // namespace pvpgn::runtime
