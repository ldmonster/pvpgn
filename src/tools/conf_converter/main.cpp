// SPDX-License-Identifier: GPL-2.0-or-later
//
// pvpgn-conf-convert: Convert legacy bnetd.conf (INI format) to TOML format.
//
// Usage:
//   pvpgn-conf-convert --input bnetd.conf --output bnetd.toml
//   pvpgn-conf-convert -i bnetd.conf -o bnetd.toml

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstring>

namespace {

struct ConversionOptions {
    std::string input_file;
    std::string output_file;
    bool verbose = false;
};

struct ConfigEntry {
    std::string key;
    std::string value;
    std::string section;  // empty for root section
};

// Trim whitespace from both ends
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Remove quotes from a value if present
std::string unquote(const std::string& str) {
    std::string s = trim(str);
    if (s.length() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.length() - 2);
    }
    return s;
}

// Escape special characters for TOML string
std::string escape_toml_string(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    result += buf;
                } else {
                    result += c;
                }
        }
    }
    return result;
}

// Parse INI-style config file
std::vector<ConfigEntry> parse_ini(const std::string& filename) {
    std::vector<ConfigEntry> entries;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open input file: " + filename);
    }
    
    std::string line;
    std::string current_section;
    int line_num = 0;
    
    while (std::getline(file, line)) {
        ++line_num;
        
        // Remove comments
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        
        line = trim(line);
        
        // Skip empty lines
        if (line.empty()) continue;
        
        // Check for section header [section]
        if (line.front() == '[' && line.back() == ']') {
            current_section = trim(line.substr(1, line.length() - 2));
            continue;
        }
        
        // Parse key = value
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            std::cerr << "Warning: Line " << line_num << " has no '=' separator: " << line << std::endl;
            continue;
        }
        
        std::string key = trim(line.substr(0, eq_pos));
        std::string value = unquote(line.substr(eq_pos + 1));
        
        if (key.empty()) {
            std::cerr << "Warning: Line " << line_num << " has empty key" << std::endl;
            continue;
        }
        
        entries.push_back({key, value, current_section});
    }
    
    return entries;
}

// Write TOML format
void write_toml(const std::string& filename, const std::vector<ConfigEntry>& entries) {
    std::ofstream file(filename);
    
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open output file: " + filename);
    }
    
    file << "# PvPGN 4.0 Configuration (TOML format)\n";
    file << "# Auto-converted from legacy INI format\n";
    file << "# Please review and adjust as needed\n\n";
    
    // Group entries by section
    std::map<std::string, std::vector<ConfigEntry>> sections;
    for (const auto& entry : entries) {
        sections[entry.section].push_back(entry);
    }
    
    // Write root section first
    if (sections.count("")) {
        for (const auto& entry : sections[""]) {
            file << entry.key << " = \"" << escape_toml_string(entry.value) << "\"\n";
        }
        file << "\n";
    }
    
    // Write other sections
    for (const auto& [section, entries_in_section] : sections) {
        if (section.empty()) continue;  // Already written
        
        file << "[" << section << "]\n";
        for (const auto& entry : entries_in_section) {
            file << entry.key << " = \"" << escape_toml_string(entry.value) << "\"\n";
        }
        file << "\n";
    }
    
    file << "# End of configuration\n";
}

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  -i, --input FILE    Input INI config file (required)\n"
              << "  -o, --output FILE   Output TOML config file (required)\n"
              << "  -v, --verbose       Enable verbose output\n"
              << "  -h, --help          Show this help message\n\n"
              << "Example:\n"
              << "  " << prog_name << " --input bnetd.conf --output bnetd.toml\n";
}

ConversionOptions parse_args(int argc, char* argv[]) {
    ConversionOptions opts;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            exit(0);
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "-i" || arg == "--input") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --input requires an argument\n";
                exit(1);
            }
            opts.input_file = argv[++i];
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --output requires an argument\n";
                exit(1);
            }
            opts.output_file = argv[++i];
        } else {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            exit(1);
        }
    }
    
    if (opts.input_file.empty() || opts.output_file.empty()) {
        std::cerr << "Error: Both --input and --output are required\n";
        print_usage(argv[0]);
        exit(1);
    }
    
    return opts;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        ConversionOptions opts = parse_args(argc, argv);
        
        if (opts.verbose) {
            std::cout << "Converting: " << opts.input_file << " -> " << opts.output_file << std::endl;
        }
        
        auto entries = parse_ini(opts.input_file);
        
        if (opts.verbose) {
            std::cout << "Parsed " << entries.size() << " configuration entries\n";
        }
        
        write_toml(opts.output_file, entries);
        
        std::cout << "Successfully converted " << opts.input_file << " to " << opts.output_file << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
