// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/file/flat_db_reader.hpp"

#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>

namespace pvpgn::infra::file {

namespace {

/// Reverse of the legacy `unescape_chars` (pvpgn-server src/common/util.cpp).
///
/// Collapses the C-style backslash escapes the original `escape_chars` emits:
///   - `\\` -> `\`     (the backslash separator is doubled on write)
///   - `\"` -> `"`     (quotes inside values are escaped)
///   - `\a \b \t \n \v \f \r` -> the corresponding control character
///   - `\ooo` (1-3 octal digits, value 1-255) -> that byte
/// Any malformed / unknown escape (including `\000`) is left verbatim, exactly
/// like the original. A trailing lone backslash is also emitted verbatim.
std::string unescape_chars(std::string_view in) {
    std::string out;
    out.reserve(in.size());

    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i] != '\\') {
            out.push_back(in[i]);
            continue;
        }
        // Lone trailing backslash: emit as-is (matches original boundary read).
        if (i + 1 >= in.size()) {
            out.push_back('\\');
            break;
        }
        const char esc = in[++i];
        switch (esc) {
            case '\\': out.push_back('\\'); break;
            case '"':  out.push_back('"');  break;
            case 'a':  out.push_back('\a'); break;
            case 'b':  out.push_back('\b'); break;
            case 't':  out.push_back('\t'); break;
            case 'n':  out.push_back('\n'); break;
            case 'v':  out.push_back('\v'); break;
            case 'f':  out.push_back('\f'); break;
            case 'r':  out.push_back('\r'); break;
            default: {
                // Octal escape: up to 3 digits in [0-7].
                std::string digits;
                std::size_t j = i;
                while (digits.size() < 3 && j < in.size() &&
                       in[j] >= '0' && in[j] <= '7') {
                    digits.push_back(in[j]);
                    ++j;
                }
                const unsigned long num =
                    digits.empty() ? 0UL : std::strtoul(digits.c_str(), nullptr, 8);
                if (digits.size() < 3 || num < 1 || num > 255) {
                    // Bad escape (including \000): leave it verbatim.
                    out.push_back('\\');
                    out.append(digits);
                } else {
                    out.push_back(static_cast<char>(num));
                }
                // `i` consumed `esc`; advance past any octal digits consumed.
                i = (j == i) ? i : j - 1;
                break;
            }
        }
    }
    return out;
}

/// Strip a single pair of surrounding ASCII double-quotes, if present.
std::string_view strip_quotes(std::string_view s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

void trim(std::string_view& s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
        s.remove_prefix(1);
    }
    while (!s.empty() &&
           (s.back() == ' ' || s.back() == '\t' ||
            s.back() == '\r' || s.back() == '\n')) {
        s.remove_suffix(1);
    }
}

}  // namespace

std::map<std::string, std::string> parse_account_file(
    std::string_view file_content) {
    std::map<std::string, std::string> result;
    std::istringstream iss{std::string{file_content}};
    std::string raw_line;

    while (std::getline(iss, raw_line)) {
        std::string_view line{raw_line};
        trim(line);  // also drops any trailing '\r' from CRLF files

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Split on the first '='.
        auto eq_pos = line.find('=');
        if (eq_pos == std::string_view::npos) {
            continue;
        }

        std::string_view key_part = line.substr(0, eq_pos);
        std::string_view val_part = line.substr(eq_pos + 1);

        // Tolerate spacing around the '=' (legacy reader allowed `" = "`).
        trim(key_part);
        trim(val_part);

        // Strip optional surrounding quotes, then unescape. This handles BOTH:
        //   - real legacy lines:  "BNET\\acct\\username"="Joe"
        //   - v3's own plain form: BNET\acct\username=Joe
        // (For an unquoted, single-backslash key/value, unescape is a no-op on
        //  the separators because they are single backslashes — but a lone
        //  backslash followed by a non-escape char is preserved verbatim.)
        const bool key_quoted = key_part.size() >= 2 &&
                                key_part.front() == '"' && key_part.back() == '"';
        const bool val_quoted = val_part.size() >= 2 &&
                                val_part.front() == '"' && val_part.back() == '"';

        std::string_view key_inner = strip_quotes(key_part);
        std::string_view val_inner = strip_quotes(val_part);

        // Only unescape when the field was quoted (legacy escaped form). v3's
        // own unquoted output uses literal single backslashes as separators and
        // must NOT be run through unescape (which would eat `\a` -> BEL, etc.).
        std::string key = key_quoted ? unescape_chars(key_inner)
                                     : std::string{key_inner};
        std::string value = val_quoted ? unescape_chars(val_inner)
                                       : std::string{val_inner};

        result[std::move(key)] = std::move(value);
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
