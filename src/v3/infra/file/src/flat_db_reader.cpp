// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/file/flat_db_reader.hpp"

#include <sstream>

namespace pvpgn::infra::file {

std::map<std::string, std::string> parse_account_file(
    std::string_view file_content) {
    std::map<std::string, std::string> result;
    std::istringstream iss{std::string{file_content}};
    std::string line;

    while (std::getline(iss, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Parse key=value
        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);

        // Trim whitespace (simple approach)
        if (!key.empty() && key.back() == ' ') {
            key.pop_back();
        }
        if (!value.empty() && value[0] == ' ') {
            value = value.substr(1);
        }

        result[key] = value;
    }

    return result;
}

std::string get_field(
    const std::map<std::string, std::string>& data,
    std::initializer_list<std::string_view> path) {
    // Build hierarchical key from path components
    std::ostringstream key_oss;
    bool first = true;
    for (auto component : path) {
        if (!first) key_oss << "\\";
        key_oss << component;
        first = false;
    }

    auto it = data.find(key_oss.str());
    return it != data.end() ? it->second : std::string{};
}

std::int64_t get_numeric_field(
    const std::map<std::string, std::string>& data,
    std::initializer_list<std::string_view> path) {
    auto value = get_field(data, path);
    if (value.empty()) {
        return 0;
    }

    try {
        return std::stoll(value);
    } catch (...) {
        return 0;
    }
}

}  // namespace pvpgn::infra::file
