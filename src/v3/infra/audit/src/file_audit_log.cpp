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

namespace ap = pvpgn::application::ports;

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

std::string escape_field(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\t': out += "\\t";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

bool unescape_field(std::string_view in, std::string& out) {
    out.clear();
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c != '\\') { out += c; continue; }
        if (++i >= in.size()) return false;
        switch (in[i]) {
            case '\\': out += '\\'; break;
            case 't':  out += '\t'; break;
            case 'n':  out += '\n'; break;
            case 'r':  out += '\r'; break;
            default:   return false;
        }
    }
    return true;
}

std::string format_iso8601_utc(core::SystemTime tp) {
    auto secs = std::chrono::time_point_cast<std::chrono::seconds>(tp);
    std::time_t t = std::chrono::system_clock::to_time_t(secs);
    std::tm     tm_buf{};
#if defined(_WIN32)
    ::gmtime_s(&tm_buf, &t);
#else
    ::gmtime_r(&t, &tm_buf);
#endif
    char buf[64];
    std::snprintf(buf, sizeof(buf),
                  "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                  tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);
    return std::string(buf);
}

bool parse_iso8601_utc(std::string_view s, core::SystemTime& out) {
    int Y, M, D, h, m, sec;
    if (s.size() != 20 || s.back() != 'Z') return false;
    if (std::sscanf(std::string(s).c_str(),
                    "%4d-%2d-%2dT%2d:%2d:%2dZ",
                    &Y, &M, &D, &h, &m, &sec) != 6) {
        return false;
    }
    std::tm tm_buf{};
    tm_buf.tm_year = Y - 1900;
    tm_buf.tm_mon  = M - 1;
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
    out = std::chrono::system_clock::from_time_t(t);
    return true;
}

}  // namespace

std::string
FileAuditLog::format_line(const ap::AuditEntry& e) {
    std::ostringstream oss;
    oss << format_iso8601_utc(e.timestamp) << '\t'
        << action_name(e.action)           << '\t'
        << e.actor.value()                 << '\t'
        << escape_field(e.subject)         << '\t'
        << escape_field(e.details)         << '\n';
    return oss.str();
}

bool
FileAuditLog::parse_line(const std::string& line, ap::AuditEntry& out) {
    // Strip trailing newline if present.
    std::string_view sv(line);
    while (!sv.empty() && (sv.back() == '\n' || sv.back() == '\r')) {
        sv.remove_suffix(1);
    }

    std::array<std::string_view, 5> fields{};
    std::size_t idx = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= sv.size(); ++i) {
        if (i == sv.size() || sv[i] == '\t') {
            if (idx >= fields.size()) return false;
            fields[idx++] = sv.substr(start, i - start);
            start = i + 1;
        }
    }
    if (idx != 5) return false;

    core::SystemTime ts;
    if (!parse_iso8601_utc(fields[0], ts)) return false;

    ap::AuditAction action;
    if (!action_from_name(fields[1], action)) return false;

    // actor id
    std::uint32_t actor_raw = 0;
    {
        std::string num(fields[2]);
        char* endp = nullptr;
        actor_raw = static_cast<std::uint32_t>(std::strtoul(num.c_str(), &endp, 10));
        if (!endp || *endp != '\0') return false;
    }

    std::string subject, details;
    if (!unescape_field(fields[3], subject)) return false;
    if (!unescape_field(fields[4], details)) return false;

    out.timestamp = ts;
    out.action    = action;
    out.actor     = pvpgn::domain::AccountId{actor_raw};
    out.subject   = std::move(subject);
    out.details   = std::move(details);
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
