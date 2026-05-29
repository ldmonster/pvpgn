// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/log/json_line_logger.hpp"

#include <cctype>
#include <chrono>
#include <cstdint>

namespace pvpgn::infra::log {

namespace {

void append_escaped(std::string& dst, std::string_view src) {
    for (char c : src) {
        unsigned char u = static_cast<unsigned char>(c);
        switch (c) {
            case '"':  dst += "\\\""; break;
            case '\\': dst += "\\\\"; break;
            case '\b': dst += "\\b";  break;
            case '\f': dst += "\\f";  break;
            case '\n': dst += "\\n";  break;
            case '\r': dst += "\\r";  break;
            case '\t': dst += "\\t";  break;
            default:
                if (u < 0x20) {
                    static const char hex[] = "0123456789abcdef";
                    dst += "\\u00";
                    dst += hex[(u >> 4) & 0xF];
                    dst += hex[u & 0xF];
                } else {
                    dst += c;
                }
        }
    }
}

std::string_view level_str(core::LogLevel l) noexcept {
    switch (l) {
        case core::LogLevel::Trace:    return "trace";
        case core::LogLevel::Debug:    return "debug";
        case core::LogLevel::Info:     return "info";
        case core::LogLevel::Warn:     return "warn";
        case core::LogLevel::Error:    return "error";
        case core::LogLevel::Critical: return "critical";
        case core::LogLevel::Off:      return "off";
    }
    return "info";
}

std::int64_t default_clock() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}

}  // namespace

JsonLineLogger::JsonLineLogger(std::ostream& out,
                               core::LogLevel min,
                               ClockFn        clock)
    : out_(&out), level_(min),
      clock_(clock ? std::move(clock) : ClockFn{&default_clock}) {}

void JsonLineLogger::log(core::LogLevel level,
                         std::string_view module,
                         std::string_view message) noexcept {
    log_kv(level, module, message, std::span<const Field>{});
}

void JsonLineLogger::log_kv(core::LogLevel level,
                            std::string_view module,
                            std::string_view message,
                            std::span<const Field> fields) noexcept {
    if (static_cast<int>(level) < static_cast<int>(level_)) return;
    try {
        std::string line;
        line.reserve(64 + module.size() + message.size()
                     + 16 * fields.size());
        line += "{\"ts\":";
        line += std::to_string(clock_());
        line += ",\"lvl\":\"";
        line += level_str(level);
        line += "\",\"mod\":\"";
        append_escaped(line, module);
        line += "\",\"msg\":\"";
        append_escaped(line, message);
        line += "\"";
        for (const auto& f : fields) {
            line += ",\"";
            append_escaped(line, f.key);
            line += "\":\"";
            append_escaped(line, f.value);
            line += "\"";
        }
        line += "}\n";
        std::lock_guard<std::mutex> lk(mu_);
        (*out_) << line;
    } catch (...) {
        // logging must not throw
    }
}

}  // namespace pvpgn::infra::log
