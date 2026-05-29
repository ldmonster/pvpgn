#include "infra/scripting/plugin/plugin_manifest.hpp"
#include <fstream>
#include <sstream>

namespace pvpgn::infra::scripting {

CapabilitySet PluginManifest::parse_capabilities() const
{
    CapabilitySet caps;
    for (const auto& cap_str : capabilities) {
        auto cap = parse_capability(cap_str);
        if (static_cast<std::uint32_t>(cap) != 0) {
            caps.grant(cap);
        }
    }
    return caps;
}

std::string load_plugin_manifest(const std::string& plugin_dir, PluginManifest& out)
{
    // For now, we'll implement a simple TOML parser
    // In production, use a proper TOML library like toml++
    
    std::string manifest_path = plugin_dir + "/plugin.toml";
    std::ifstream file(manifest_path);
    
    if (!file.is_open()) {
        return "Failed to open plugin.toml at " + manifest_path;
    }
    
    out.plugin_dir = plugin_dir;
    out.plugin_type = "lua"; // default
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;
        
        // Simple key = value parsing
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;
        
        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // Remove quotes
        if (value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.length() - 2);
        }
        
        if (key == "name") {
            out.name = value;
        } else if (key == "version") {
            out.version = value;
        } else if (key == "api") {
            out.api_version = value;
        } else if (key == "entry") {
            out.entry = value;
            // Determine plugin type from entry
            if (value.find(".lua") != std::string::npos) {
                out.plugin_type = "lua";
            } else if (value.find(".so") != std::string::npos || value.find(".dll") != std::string::npos) {
                out.plugin_type = "native";
            }
        } else if (key == "description") {
            out.description = value;
        } else if (key == "authors") {
            // Parse comma-separated authors
            std::istringstream iss(value);
            std::string author;
            while (std::getline(iss, author, ',')) {
                author.erase(0, author.find_first_not_of(" \t"));
                author.erase(author.find_last_not_of(" \t") + 1);
                if (!author.empty()) {
                    out.authors.push_back(author);
                }
            }
        } else if (key == "capabilities") {
            // Parse comma-separated capabilities
            std::istringstream iss(value);
            std::string cap;
            while (std::getline(iss, cap, ',')) {
                cap.erase(0, cap.find_first_not_of(" \t"));
                cap.erase(cap.find_last_not_of(" \t") + 1);
                if (!cap.empty()) {
                    out.capabilities.push_back(cap);
                }
            }
        }
    }
    
    // Validate required fields
    if (out.name.empty()) {
        return "Missing required field: name";
    }
    if (out.entry.empty()) {
        return "Missing required field: entry";
    }
    if (out.api_version.empty()) {
        return "Missing required field: api";
    }
    
    return ""; // success
}

} // namespace pvpgn::infra::scripting
