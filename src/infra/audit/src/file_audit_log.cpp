// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/audit/file_audit_log.hpp"

#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string_view>
#include <vector>

namespace pvpgn::infra::audit {

namespace ap = pvpgn::domain::moderation;

namespace {

constexpr std::string_view action_name(ap::AuditAction a) noexcept {
    switch (a) {
        case ap::AuditAction::AccountCreated:      return "AccountCreated";
        case ap::AuditAction::AccountLocked:       return "AccountLocked";
        case ap::AuditAction::AccountBanned:       return "AccountBanned";
        case ap::AuditAction::AccountUnbanned:     return "AccountUnbanned";
        case ap::AuditAction::IpBanned:            return "IpBanned";
        case ap::AuditAction::IpUnbanned:          return "IpUnbanned";
        case ap::AuditAction::ChannelCreated:      return "ChannelCreated";
        case ap::AuditAction::ChannelDeleted:      return "ChannelDeleted";
        case ap::AuditAction::ChannelMemberKicked: return "ChannelMemberKicked";
        case ap::AuditAction::MemberBanned:        return "MemberBanned";
        case ap::AuditAction::TopicChanged:        return "TopicChanged";
        case ap::AuditAction::GameCreated:         return "GameCreated";
        case ap::AuditAction::GameEnded:           return "GameEnded";
        case ap::AuditAction::GameResultReported:  return "GameResultReported";
        case ap::AuditAction::ClanCreated:         return "ClanCreated";
        case ap::AuditAction::ClanDisbanded:       return "ClanDisbanded";
        case ap::AuditAction::MemberPromoted:      return "MemberPromoted";
        case ap::AuditAction::ClanMemberKicked:    return "ClanMemberKicked";
        case ap::AuditAction::ServerShutdown:      return "ServerShutdown";
        case ap::AuditAction::ConfigReloaded:      return "ConfigReloaded";
    }
    return "Unknown";
}

bool action_from_name(std::string_view s, ap::AuditAction& out) noexcept {
    for (std::uint16_t i = 0;
         i <= static_cast<std::uint16_t>(ap::AuditAction::ConfigReloaded); ++i) {
        auto a = static_cast<ap::AuditAction>(i);
        if (action_name(a) == s) { out = a; return true; }
    }
    return false;
}

/// Escape a string for embedding inside a JSON double-quoted value.
/// Escapes: `"` → `\"`, `\` → `\\`, `\n` → `\n`, `\r` → `\r`, `\t` → `\t`.
/// Other control characters (< 0x20) are emitted as `\uXXXX`.
std::string json_escape(std::string_view in) {
    std::string out;
    out.reserve(in.size() + 4);
    for (char ch : in) {
        const unsigned char c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned>(c));
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
                break;
        }
    }
    return out;
}

/// Unescape a JSON string value (content between the outer quotes).
/// Handles `\"`, `\\`, `\/`, `\n`, `\r`, `\t`, `\b`, `\f`, `\uXXXX`.
/// Returns false on malformed escape sequences.
bool json_unescape(std::string_view in, std::string& out) {
    out.clear();
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c != '\\') { out += c; continue; }
        if (++i >= in.size()) return false;
        switch (in[i]) {
            case '"':  out += '"';  break;
            case '\\': out += '\\'; break;
            case '/':  out += '/';  break;
            case 'n':  out += '\n'; break;
            case 'r':  out += '\r'; break;
            case 't':  out += '\t'; break;
            case 'b':  out += '\b'; break;
            case 'f':  out += '\f'; break;
            case 'u': {
                if (i + 4 >= in.size()) return false;
                char hex[5] = { in[i+1], in[i+2], in[i+3], in[i+4], '\0' };
                char* endp = nullptr;
                unsigned long cp = std::strtoul(hex, &endp, 16);
                if (!endp || *endp != '\0') return false;
                // Only handle BMP codepoints (U+0000–U+FFFF) as single bytes
                // for the ASCII subset we write; non-ASCII passthrough as-is.
                if (cp < 0x80) {
                    out += static_cast<char>(cp);
                } else {
                    // Re-emit as UTF-8 (basic BMP only)
                    if (cp < 0x800) {
                        out += static_cast<char>(0xC0 | (cp >> 6));
                        out += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        out += static_cast<char>(0xE0 | (cp >> 12));
                        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        out += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                }
                i += 4;
                break;
            }
            default: return false;
        }
    }
    return true;
}

/// Format a SystemTime as ISO 8601 UTC with milliseconds:
/// "2024-01-01T00:00:00.000Z"
std::string format_iso8601_ms_utc(core::SystemTime tp) {
    auto ms_since_epoch =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            tp.time_since_epoch());
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(ms_since_epoch);
    auto ms   = ms_since_epoch - secs;

    std::time_t t = static_cast<std::time_t>(secs.count());
    std::tm tm_buf{};
#if defined(_WIN32)
    ::gmtime_s(&tm_buf, &t);
#else
    ::gmtime_r(&t, &tm_buf);
#endif
    char buf[64];
    std::snprintf(buf, sizeof(buf),
                  "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                  tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                  tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                  static_cast<int>(ms.count()));
    return std::string(buf);
}

/// Parse ISO 8601 UTC timestamp with optional milliseconds.
/// Accepts both "2024-01-01T00:00:00Z" and "2024-01-01T00:00:00.000Z".
bool parse_iso8601_utc(std::string_view s, core::SystemTime& out) {
    int Y = 0, Mo = 0, D = 0, h = 0, m = 0, sec = 0, ms = 0;
    // Try with milliseconds first (length 24: "YYYY-MM-DDTHH:MM:SS.mmmZ")
    if (s.size() == 24 && s.back() == 'Z') {
        if (std::sscanf(std::string(s).c_str(),
                        "%4d-%2d-%2dT%2d:%2d:%2d.%3dZ",
                        &Y, &Mo, &D, &h, &m, &sec, &ms) != 7) {
            return false;
        }
    } else if (s.size() == 20 && s.back() == 'Z') {
        if (std::sscanf(std::string(s).c_str(),
                        "%4d-%2d-%2dT%2d:%2d:%2dZ",
                        &Y, &Mo, &D, &h, &m, &sec) != 6) {
            return false;
        }
        ms = 0;
    } else {
        return false;
    }
    std::tm tm_buf{};
    tm_buf.tm_year = Y - 1900;
    tm_buf.tm_mon  = Mo - 1;
    tm_buf.tm_mday = D;
    tm_buf.tm_hour = h;
    tm_buf.tm_min  = m;
    tm_buf.tm_sec  = sec;
#if defined(_WIN32)
    std::time_t t = ::_mkgmtime(&tm_buf);
#else
    std::time_t t = ::timegm(&tm_buf);
#endif
    if (t == static_cast<std::time_t>(-1)) return false;
    out = std::chrono::system_clock::from_time_t(t)
        + std::chrono::milliseconds(ms);
    return true;
}

/// Extract the content of a JSON string field starting at position `pos`
/// (which must point to the opening `"`). On success, advances `pos` past
/// the closing `"` and returns true.
bool extract_json_string(std::string_view sv, std::size_t& pos,
                         std::string& value) {
    if (pos >= sv.size() || sv[pos] != '"') return false;
    ++pos;  // skip opening quote
    std::size_t start = pos;
    // Find closing quote, respecting backslash escapes
    while (pos < sv.size() && sv[pos] != '"') {
        if (sv[pos] == '\\') {
            ++pos;  // skip escape char
            if (pos >= sv.size()) return false;
        }
        ++pos;
    }
    if (pos >= sv.size()) return false;  // no closing quote
    std::string_view raw = sv.substr(start, pos - start);
    ++pos;  // skip closing quote
    return json_unescape(raw, value);
}

/// Skip whitespace at `pos`.
void skip_ws(std::string_view sv, std::size_t& pos) {
    while (pos < sv.size() &&
           (sv[pos] == ' ' || sv[pos] == '\t' ||
            sv[pos] == '\r' || sv[pos] == '\n')) {
        ++pos;
    }
}

/// Expect character `c` at `pos`, advance past it. Returns false if mismatch.
bool expect_char(std::string_view sv, std::size_t& pos, char c) {
    skip_ws(sv, pos);
    if (pos >= sv.size() || sv[pos] != c) return false;
    ++pos;
    return true;
}

}  // namespace

std::string
FileAuditLog::format_line(const ap::AuditEntry& e) {
    std::ostringstream oss;
    oss << "{\"ts\":\""      << format_iso8601_ms_utc(e.timestamp) << "\""
        << ",\"action\":\"" << action_name(e.action)              << "\""
        << ",\"actor\":"    << e.actor.value()
        << ",\"subject\":\"" << json_escape(e.subject)            << "\""
        << ",\"details\":\"" << json_escape(e.details)            << "\"";
    if (!e.source_ip.empty()) {
        oss << ",\"source_ip\":\"" << json_escape(e.source_ip) << "\"";
    }
    oss << "}\n";
    return oss.str();
}

bool
FileAuditLog::parse_line(const std::string& line, ap::AuditEntry& out) {
    // Strip trailing newline(s).
    std::string_view sv(line);
    while (!sv.empty() && (sv.back() == '\n' || sv.back() == '\r')) {
        sv.remove_suffix(1);
    }

    // Expect opening '{'
    std::size_t pos = 0;
    if (!expect_char(sv, pos, '{')) return false;

    // We parse key-value pairs in any order until '}'.
    // Required fields: ts, action, actor, subject, details.
    // Optional: source_ip.
    bool has_ts = false, has_action = false, has_actor = false,
         has_subject = false, has_details = false;

    core::SystemTime    ts{};
    ap::AuditAction     action{};
    std::uint32_t       actor_raw = 0;
    std::string         subject, details, source_ip;

    while (true) {
        skip_ws(sv, pos);
        if (pos >= sv.size()) return false;
        if (sv[pos] == '}') { ++pos; break; }

        // Expect a key string
        std::string key;
        if (!extract_json_string(sv, pos, key)) return false;

        // Expect ':'
        if (!expect_char(sv, pos, ':')) return false;
        skip_ws(sv, pos);

        if (key == "ts") {
            std::string ts_str;
            if (!extract_json_string(sv, pos, ts_str)) return false;
            if (!parse_iso8601_utc(ts_str, ts)) return false;
            has_ts = true;
        } else if (key == "action") {
            std::string action_str;
            if (!extract_json_string(sv, pos, action_str)) return false;
            if (!action_from_name(action_str, action)) return false;
            has_action = true;
        } else if (key == "actor") {
            // Numeric value (no quotes)
            std::size_t num_start = pos;
            while (pos < sv.size() &&
                   sv[pos] >= '0' && sv[pos] <= '9') {
                ++pos;
            }
            if (pos == num_start) return false;
            std::string num(sv.substr(num_start, pos - num_start));
            char* endp = nullptr;
            actor_raw = static_cast<std::uint32_t>(
                std::strtoul(num.c_str(), &endp, 10));
            if (!endp || *endp != '\0') return false;
            has_actor = true;
        } else if (key == "subject") {
            if (!extract_json_string(sv, pos, subject)) return false;
            has_subject = true;
        } else if (key == "details") {
            if (!extract_json_string(sv, pos, details)) return false;
            has_details = true;
        } else if (key == "source_ip") {
            if (!extract_json_string(sv, pos, source_ip)) return false;
        } else {
            // Unknown key — skip its value (string or number only for now)
            skip_ws(sv, pos);
            if (pos < sv.size() && sv[pos] == '"') {
                std::string ignored;
                if (!extract_json_string(sv, pos, ignored)) return false;
            } else {
                // Skip until comma or '}'
                while (pos < sv.size() &&
                       sv[pos] != ',' && sv[pos] != '}') {
                    ++pos;
                }
            }
        }

        // After value, expect ',' or '}'
        skip_ws(sv, pos);
        if (pos >= sv.size()) return false;
        if (sv[pos] == ',') { ++pos; continue; }
        if (sv[pos] == '}') { ++pos; break; }
        return false;
    }

    if (!has_ts || !has_action || !has_actor || !has_subject || !has_details) {
        return false;
    }

    out.timestamp = ts;
    out.action    = action;
    out.actor     = pvpgn::domain::AccountId{actor_raw};
    out.subject   = std::move(subject);
    out.details   = std::move(details);
    out.source_ip = std::move(source_ip);
    return true;
}

FileAuditLog::FileAuditLog(const std::string& path) : path_(path) {
    fp_ = std::fopen(path.c_str(), "ab");
    assert(fp_ && "FileAuditLog: failed to open path for append");
}

FileAuditLog::~FileAuditLog() {
    if (fp_) std::fclose(fp_);
}

std::unique_ptr<FileAuditLog>
FileAuditLog::try_open(const std::string& path) {
    auto* fp = std::fopen(path.c_str(), "ab");
    if (!fp) return nullptr;
    std::fclose(fp);
    return std::unique_ptr<FileAuditLog>(new FileAuditLog(path));
}

void
FileAuditLog::record(const ap::AuditEntry& entry) {
    std::string line = format_line(entry);
    std::lock_guard<std::mutex> lk(mutex_);
    if (!fp_) return;
    std::fwrite(line.data(), 1, line.size(), fp_);
    std::fflush(fp_);
}

std::vector<ap::AuditEntry>
FileAuditLog::recent(std::size_t count) const {
    std::vector<ap::AuditEntry> out;
    if (count == 0) return out;

    std::lock_guard<std::mutex> lk(mutex_);
    if (fp_) std::fflush(fp_);

    std::ifstream in(path_, std::ios::binary);
    if (!in) return out;

    // Naive: read all lines, keep last `count`. Audit logs are not
    // expected to be huge in practice; if they are, the in-memory
    // adapter should be used for queries.
    std::vector<std::string> lines;
    std::string              cur;
    while (std::getline(in, cur)) {
        lines.push_back(std::move(cur));
        cur.clear();
    }

    std::size_t start = lines.size() > count ? lines.size() - count : 0;
    for (std::size_t i = start; i < lines.size(); ++i) {
        ap::AuditEntry e{};
        if (parse_line(lines[i], e)) {
            out.push_back(std::move(e));
        }
    }
    return out;
}

}  // namespace pvpgn::infra::audit
