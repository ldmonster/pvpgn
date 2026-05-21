// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file string_utils.hpp
/// Pure C++20 string / time-string utility helpers.
///
/// Migrated from src/common/util.{h,cpp} (R103).
///
/// Functions with FILE* I/O (file_get_line, str_print_term) are intentionally
/// NOT migrated here — they belong in an I/O layer, not a pure utility header.
///
/// All functions live in namespace pvpgn::core.
/// This header is intentionally header-only (no .cpp needed).

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace pvpgn::core {

// ---------------------------------------------------------------------------
// str_starts_with_word
//
// Returns true if `full` starts with `part` (case-insensitive) AND the
// character immediately after `part` in `full` is either a space, tab, or
// the end of the string.  Mirrors legacy pvpgn::strstart() == 0.
//
// Legacy: strstart(full, part) returns 0 on match, non-zero otherwise.
// v3:     returns bool — true on match.
// ---------------------------------------------------------------------------
[[nodiscard]] inline bool str_starts_with_word(std::string_view full,
                                               std::string_view part) noexcept {
    if (full.size() < part.size()) return false;
    for (std::size_t i = 0; i < part.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(full[i])) !=
            std::tolower(static_cast<unsigned char>(part[i])))
            return false;
    }
    if (full.size() == part.size()) return true;
    const char next = full[part.size()];
    return next == ' ' || next == '\t';
}

// ---------------------------------------------------------------------------
// str_reverse
//
// Returns a copy of `s` with its characters reversed.
// Mirrors legacy pvpgn::strreverse() but returns std::string instead of
// mutating a char*.
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::string str_reverse(std::string_view s) {
    std::string result{s};
    std::reverse(result.begin(), result.end());
    return result;
}

// ---------------------------------------------------------------------------
// str_to_uint / str_to_ushort
//
// Parse an unsigned integer from a string.  Leading whitespace and a leading
// '+' are accepted.  Returns std::nullopt on overflow or invalid input.
// Mirrors legacy pvpgn::str_to_uint / str_to_ushort (return 0 on success,
// -1 on failure) but uses std::optional for cleaner error signalling.
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::optional<unsigned int>
str_to_uint(std::string_view sv) noexcept {
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t'))
        sv.remove_prefix(1);
    if (!sv.empty() && sv.front() == '+')
        sv.remove_prefix(1);
    if (sv.empty()) return std::nullopt;

    unsigned int val{};
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
    if (ec != std::errc{} || ptr != sv.data() + sv.size())
        return std::nullopt;
    return val;
}

[[nodiscard]] inline std::optional<unsigned short>
str_to_ushort(std::string_view sv) noexcept {
    auto opt = str_to_uint(sv);
    if (!opt) return std::nullopt;
    if (*opt > static_cast<unsigned int>(
                   std::numeric_limits<unsigned short>::max()))
        return std::nullopt;
    return static_cast<unsigned short>(*opt);
}

// ---------------------------------------------------------------------------
// str_get_bool
//
// Parse a boolean string.  Recognised true values: "true", "yes", "on", "1".
// Recognised false values: "false", "no", "off", "0".
// Returns std::nullopt for unrecognised input.
// Mirrors legacy pvpgn::str_get_bool() (1/0/-1) but uses std::optional<bool>.
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::optional<bool>
str_get_bool(std::string_view sv) noexcept {
    auto iequal = [](std::string_view a, std::string_view b) noexcept -> bool {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i])))
                return false;
        return true;
    };

    if (iequal(sv, "true") || iequal(sv, "yes") || iequal(sv, "on") ||
        sv == "1")
        return true;
    if (iequal(sv, "false") || iequal(sv, "no") || iequal(sv, "off") ||
        sv == "0")
        return false;
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// seconds_to_timestr
//
// Format a duration in seconds as a human-readable string, e.g.
//   "2 days 3 hours 4 minutes 5 seconds"
//   "3 hours 4 minutes 5 seconds"
//   "4 minutes 5 seconds"
//   "5 seconds."   (note trailing period when only seconds remain)
//
// Mirrors legacy pvpgn::seconds_to_timestr() but returns std::string instead
// of a pointer to a static buffer.
// ---------------------------------------------------------------------------
namespace detail {

inline void append_uint(std::string& out, unsigned int n) {
    if (n == 0) { out += '0'; return; }
    char buf[12];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), n);
    out.append(buf, ptr);
}

} // namespace detail

[[nodiscard]] inline std::string seconds_to_timestr(unsigned int totsecs) {
    const unsigned int days    = totsecs / (24u * 60u * 60u);
    const unsigned int hours   = (totsecs / (60u * 60u)) % 24u;
    const unsigned int minutes = (totsecs / 60u) % 60u;
    const unsigned int seconds = totsecs % 60u;

    auto plural = [](unsigned int n) -> std::string_view {
        return n == 1u ? "" : "s";
    };

    std::string out;
    out.reserve(64);

    if (days > 0) {
        detail::append_uint(out, days);
        out += " day"; out += plural(days); out += ' ';
        detail::append_uint(out, hours);
        out += " hour"; out += plural(hours); out += ' ';
        detail::append_uint(out, minutes);
        out += " minute"; out += plural(minutes); out += ' ';
        detail::append_uint(out, seconds);
        out += " second"; out += plural(seconds);
    } else if (hours > 0) {
        detail::append_uint(out, hours);
        out += " hour"; out += plural(hours); out += ' ';
        detail::append_uint(out, minutes);
        out += " minute"; out += plural(minutes); out += ' ';
        detail::append_uint(out, seconds);
        out += " second"; out += plural(seconds);
    } else if (minutes > 0) {
        detail::append_uint(out, minutes);
        out += " minute"; out += plural(minutes); out += ' ';
        detail::append_uint(out, seconds);
        out += " second"; out += plural(seconds);
    } else {
        detail::append_uint(out, seconds);
        out += " second"; out += plural(seconds); out += '.';
    }
    return out;
}

// ---------------------------------------------------------------------------
// clockstr_to_seconds
//
// Parse a clock string of the form "HH:MM:SS" or "MM:SS" into a total
// number of seconds.  Returns std::nullopt on invalid input.
// Mirrors legacy pvpgn::clockstr_to_seconds().
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::optional<unsigned int>
clockstr_to_seconds(std::string_view sv) noexcept {
    if (sv.empty()) return std::nullopt;

    unsigned int total    = 0u;
    std::size_t  seg_start = 0u;

    for (std::size_t j = 0u; j <= sv.size(); ++j) {
        const bool at_sep = (j < sv.size() && sv[j] == ':');
        const bool at_end = (j == sv.size());
        if (!at_sep && !at_end) {
            if (sv[j] < '0' || sv[j] > '9') return std::nullopt;
            continue;
        }
        unsigned int seg_val = 0u;
        auto [ptr, ec] = std::from_chars(sv.data() + seg_start,
                                          sv.data() + j, seg_val);
        if (ec != std::errc{}) return std::nullopt;
        total = total * 60u + seg_val;
        seg_start = j + 1u;
    }
    return total;
}

// ---------------------------------------------------------------------------
// escape_fs_chars
//
// Percent-encode characters that are illegal in filesystem paths:
//   NUL, '%', '/', '\\', ':'
// Each such byte becomes %XX (uppercase hex).
// Mirrors legacy pvpgn::escape_fs_chars() but returns std::string.
// ---------------------------------------------------------------------------
namespace detail {

inline constexpr char kHexUpper[] = "0123456789ABCDEF";

inline void append_percent_hex(std::string& out, unsigned char c) {
    out += '%';
    out += kHexUpper[c >> 4];
    out += kHexUpper[c & 0x0f];
}

} // namespace detail

[[nodiscard]] inline std::string escape_fs_chars(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (const unsigned char c : in) {
        if (c == '\0' || c == '%' || c == '/' || c == '\\' || c == ':')
            detail::append_percent_hex(out, c);
        else
            out += static_cast<char>(c);
    }
    return out;
}

// ---------------------------------------------------------------------------
// escape_chars / unescape_chars
//
// C-style backslash escaping for printable strings stored in config files.
//
// escape_chars: converts non-printable bytes to \a \b \t \n \v \f \r \\
//               \" or \OOO (3-digit octal).  Printable chars pass through.
// unescape_chars: reverses the above.
//
// Both mirror the legacy pvpgn::escape_chars / unescape_chars but return
// std::string instead of heap-allocated char*.
// ---------------------------------------------------------------------------
namespace detail {

inline void append_octal3(std::string& out, unsigned char c) {
    out += '\\';
    out += static_cast<char>('0' + ((c >> 6) & 0x7));
    out += static_cast<char>('0' + ((c >> 3) & 0x7));
    out += static_cast<char>('0' + ( c       & 0x7));
}

} // namespace detail

[[nodiscard]] inline std::string escape_chars(std::string_view in) {
    std::string out;
    out.reserve(in.size() * 2u);
    for (const unsigned char c : in) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\a': out += "\\a";  break;
        case '\b': out += "\\b";  break;
        case '\t': out += "\\t";  break;
        case '\n': out += "\\n";  break;
        case '\v': out += "\\v";  break;
        case '\f': out += "\\f";  break;
        case '\r': out += "\\r";  break;
        default:
            if (std::isprint(c))
                out += static_cast<char>(c);
            else
                detail::append_octal3(out, c);
            break;
        }
    }
    return out;
}

[[nodiscard]] inline std::string unescape_chars(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0u; i < in.size(); ++i) {
        if (in[i] != '\\') { out += in[i]; continue; }
        ++i;
        if (i >= in.size()) { out += '\\'; break; }
        switch (in[i]) {
        case '\\': out += '\\'; break;
        case '"':  out += '"';  break;
        case 'a':  out += '\a'; break;
        case 'b':  out += '\b'; break;
        case 't':  out += '\t'; break;
        case 'n':  out += '\n'; break;
        case 'v':  out += '\v'; break;
        case 'f':  out += '\f'; break;
        case 'r':  out += '\r'; break;
        default: {
            // Try to parse up to 3 octal digits
            const std::size_t oct_start = i;
            std::size_t oct_len = 0u;
            while (oct_len < 3u && (i + oct_len) < in.size()) {
                const char d = in[i + oct_len];
                if (d < '0' || d > '7') break;
                ++oct_len;
            }
            if (oct_len == 3u) {
                unsigned int num = 0u;
                std::from_chars(in.data() + oct_start,
                                in.data() + oct_start + 3u, num, 8);
                if (num >= 1u && num <= 255u) {
                    out += static_cast<char>(static_cast<unsigned char>(num));
                    i += 2u; // loop will ++i once more → +3 total
                    break;
                }
            }
            // Bad escape — emit literally
            out += '\\';
            out += in[i];
            break;
        }
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// bytes_to_hex_str / hex_str_to_bytes
//
// Convert a raw byte buffer to/from a space-separated uppercase hex string
// (e.g. "DE AD BE EF ").  Mirrors legacy pvpgn::str_to_hex / hex_to_str.
//
// bytes_to_hex_str: each byte → "XX " (3 chars, trailing space).
// hex_str_to_bytes: parses the space-separated format back to bytes.
//                   Returns the decoded bytes, or std::nullopt on parse error.
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::string bytes_to_hex_str(std::string_view data) {
    std::string out;
    out.reserve(data.size() * 3u);
    for (const unsigned char c : data) {
        out += detail::kHexUpper[c >> 4];
        out += detail::kHexUpper[c & 0x0f];
        out += ' ';
    }
    return out;
}

[[nodiscard]] inline std::optional<std::string>
hex_str_to_bytes(std::string_view source, std::size_t datalen) {
    std::string out;
    out.reserve(datalen);
    for (std::size_t i = 0u; i < datalen; ++i) {
        if (source.size() < i * 3u + 2u) return std::nullopt;
        unsigned int byte_val = 0u;
        auto [ptr, ec] = std::from_chars(source.data() + i * 3u,
                                          source.data() + i * 3u + 2u,
                                          byte_val, 16);
        if (ec != std::errc{}) return std::nullopt;
        out += static_cast<char>(static_cast<unsigned char>(byte_val));
    }
    return out;
}

// ---------------------------------------------------------------------------
// timestr_to_time
//
// Parse a date/time string into a std::time_t.
// Accepted formats (separators '/', '-', '.'):
//   "yyyy/mm/dd"
//   "hh:mm:ss"
//   "yyyy/mm/dd hh:mm:ss"
//   "yyyy-mm-dd hh:mm:ss"
//   "yyyy.mm.dd hh:mm:ss"
// An empty string returns time_t{0}.
// Returns std::nullopt if mktime() fails.
// Mirrors legacy pvpgn::timestr_to_time().
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::optional<std::time_t>
timestr_to_time(std::string_view sv) noexcept {
    if (sv.empty()) return std::time_t{0};

    std::tm when{};
    when.tm_mday  = 1;
    when.tm_isdst = -1;

    // Replicate the legacy state-machine parser exactly.
    int day_s = 0, time_s = 0, last = 0;
    std::size_t seg_start = 0u;

    // Work on a NUL-terminated copy so std::atoi is safe.
    std::string s{sv};
    s += '\0';

    for (std::size_t k = 0u; k < s.size(); ++k) {
        const char ch = s[k];
        switch (ch) {
        case '/': case '-': case '.': {
            const int val = std::atoi(s.c_str() + seg_start);
            if      (day_s == 0) when.tm_year = val - 1900;
            else if (day_s == 1) when.tm_mon  = val - 1;
            else if (day_s == 2) when.tm_mday = val;
            time_s = 0; ++day_s;
            seg_start = k + 1u;
            last = 1;
            break;
        }
        case ':': {
            const int val = std::atoi(s.c_str() + seg_start);
            if      (time_s == 0) when.tm_hour = val;
            else if (time_s == 1) when.tm_min  = val;
            else if (time_s == 2) when.tm_sec  = val;
            day_s = 0; ++time_s;
            seg_start = k + 1u;
            last = 2;
            break;
        }
        case ' ': case '\t': case '\0': {
            if (last == 1) {
                const int val = std::atoi(s.c_str() + seg_start);
                if      (day_s == 0) when.tm_year = val - 1900;
                else if (day_s == 1) when.tm_mon  = val - 1;
                else if (day_s == 2) when.tm_mday = val;
            } else if (last == 2) {
                const int val = std::atoi(s.c_str() + seg_start);
                if      (time_s == 0) when.tm_hour = val;
                else if (time_s == 1) when.tm_min  = val;
                else if (time_s == 2) when.tm_sec  = val;
            }
            time_s = day_s = 0;
            seg_start = k + 1u;
            last = 0;
            break;
        }
        default:
            break;
        }
        if (ch == '\0') break;
    }

    const std::time_t t = std::mktime(&when);
    if (t == static_cast<std::time_t>(-1)) return std::nullopt;
    return t;
}

// ---------------------------------------------------------------------------
// str_skip_space / str_skip_word
//
// string_view equivalents of the legacy inline helpers.
// str_skip_space: remove leading spaces and tabs.
// str_skip_word:  remove leading non-space, non-tab characters.
// ---------------------------------------------------------------------------
[[nodiscard]] constexpr std::string_view
str_skip_space(std::string_view sv) noexcept {
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t'))
        sv.remove_prefix(1);
    return sv;
}

[[nodiscard]] constexpr std::string_view
str_skip_word(std::string_view sv) noexcept {
    while (!sv.empty() && sv.front() != ' ' && sv.front() != '\t')
        sv.remove_prefix(1);
    return sv;
}

} // namespace pvpgn::core
