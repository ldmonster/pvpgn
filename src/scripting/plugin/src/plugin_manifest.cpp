// SPDX-License-Identifier: GPL-2.0-or-later
#include "scripting/plugin/plugin_manifest.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <string>

namespace pvpgn::scripting::plugin {

namespace {

/// Trim whitespace from both ends of a string
std::string_view trim(std::string_view str) {
    // Returns a view that narrows the *input* range — it must NOT allocate a
    // std::string. Returning std::string and then doing `sv = trim(sv)` (as
    // unquote did) leaves the view dangling into a destroyed temporary
    // (heap-use-after-free). A view into the caller's still-live buffer is
    // both correct and allocation-free.
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }
    size_t end = str.size();
    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }
    return str.substr(start, end - start);
}

/// Remove quotes from a string value
std::string unquote(std::string_view str) {
    str = trim(str);
    if (str.size() >= 2 && str[0] == '"' && str[str.size() - 1] == '"') {
        return std::string(str.substr(1, str.size() - 2));
    }
    return std::string(str);
}

/// Parse a TOML array of strings: ["item1", "item2"]
std::vector<std::string> parse_string_array(std::string_view value_str) {
    std::vector<std::string> result;
    value_str = trim(value_str);
    
    if (value_str.empty() || value_str[0] != '[') {
        return result;
    }
    
    size_t end = value_str.rfind(']');
    if (end == std::string::npos) {
        return result;
    }
    
    std::string content = std::string(value_str.substr(1, end - 1));
    
    // Split by comma
    size_t pos = 0;
    while (pos < content.size()) {
        size_t comma = content.find(',', pos);
        if (comma == std::string::npos) {
            comma = content.size();
        }
        std::string item = unquote(content.substr(pos, comma - pos));
        if (!item.empty()) {
            result.push_back(item);
        }
        pos = comma + 1;
    }
    
    return result;
}

} // namespace

core::Result<PluginManifest, core::Error> PluginManifest::parse_toml(std::string_view toml_content) {
    PluginManifest manifest;
    std::string content(toml_content);
    bool in_plugin_section = false;
    bool in_dependencies_section = false;
    PluginDependency current_dep;

    // Split by newlines manually
    size_t pos = 0;
    while (pos < content.size()) {
        size_t newline = content.find('\n', pos);
        if (newline == std::string::npos) {
            newline = content.size();
        }
        
        std::string line{trim(content.substr(pos, newline - pos))};
        pos = newline + 1;

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Check for section headers
        if (line == "[plugin]") {
            in_plugin_section = true;
            in_dependencies_section = false;
            continue;
        }
        if (line == "[[dependencies]]") {
            in_dependencies_section = true;
            in_plugin_section = false;
            // Save previous dependency if any
            if (!current_dep.plugin_id.empty()) {
                manifest.dependencies.push_back(current_dep);
                current_dep = PluginDependency();
            }
            continue;
        }

        // Parse key = value
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }

        std::string key{trim(line.substr(0, eq_pos))};
        std::string value{trim(line.substr(eq_pos + 1))};

        if (in_plugin_section) {
            if (key == "id") {
                manifest.id = unquote(value);
            } else if (key == "name") {
                manifest.name = unquote(value);
            } else if (key == "version") {
                auto ver = SemVer::parse(unquote(value));
                if (!ver.has_value()) {
                    return core::fail(ver.error());
                }
                manifest.version = std::move(ver).value();
            } else if (key == "description") {
                manifest.description = unquote(value);
            } else if (key == "author") {
                manifest.author = unquote(value);
            } else if (key == "license") {
                manifest.license = unquote(value);
            } else if (key == "entry_point") {
                manifest.entry_point = unquote(value);
            } else if (key == "api_version_req") {
                manifest.api_version_req = unquote(value);
            } else if (key == "provides") {
                manifest.provides = parse_string_array(value);
            } else if (key == "conflicts") {
                manifest.conflicts = parse_string_array(value);
            }
        } else if (in_dependencies_section) {
            if (key == "plugin_id") {
                current_dep.plugin_id = unquote(value);
            } else if (key == "version_req") {
                current_dep.version_req = unquote(value);
            } else if (key == "optional") {
                current_dep.optional = (value == "true");
            }
        }
    }

    // Save last dependency if any
    if (!current_dep.plugin_id.empty()) {
        manifest.dependencies.push_back(current_dep);
    }

    // Validate required fields
    if (manifest.id.empty()) {
        return core::fail(core::Error(core::StatusCode::InvalidArgument,
            "Plugin manifest missing required field: id"));
    }
    if (manifest.name.empty()) {
        return core::fail(core::Error(core::StatusCode::InvalidArgument,
            "Plugin manifest missing required field: name"));
    }
    if (manifest.entry_point.empty()) {
        return core::fail(core::Error(core::StatusCode::InvalidArgument,
            "Plugin manifest missing required field: entry_point"));
    }

    return manifest;
}

std::string PluginManifest::to_toml() const {
    std::ostringstream oss;

    oss << "[plugin]\n";
    oss << "id = \"" << id << "\"\n";
    oss << "name = \"" << name << "\"\n";
    oss << "version = \"" << version.to_string() << "\"\n";
    if (!description.empty()) {
        oss << "description = \"" << description << "\"\n";
    }
    if (!author.empty()) {
        oss << "author = \"" << author << "\"\n";
    }
    if (!license.empty()) {
        oss << "license = \"" << license << "\"\n";
    }
    oss << "entry_point = \"" << entry_point << "\"\n";
    if (!api_version_req.empty()) {
        oss << "api_version_req = \"" << api_version_req << "\"\n";
    }

    if (!provides.empty()) {
        oss << "provides = [";
        for (size_t i = 0; i < provides.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "\"" << provides[i] << "\"";
        }
        oss << "]\n";
    }

    if (!conflicts.empty()) {
        oss << "conflicts = [";
        for (size_t i = 0; i < conflicts.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "\"" << conflicts[i] << "\"";
        }
        oss << "]\n";
    }

    for (const auto& dep : dependencies) {
        oss << "\n[[dependencies]]\n";
        oss << "plugin_id = \"" << dep.plugin_id << "\"\n";
        oss << "version_req = \"" << dep.version_req << "\"\n";
        oss << "optional = " << (dep.optional ? "true" : "false") << "\n";
    }

    return oss.str();
}

} // namespace pvpgn::scripting::plugin
