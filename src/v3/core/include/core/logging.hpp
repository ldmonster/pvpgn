// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file logging.hpp
/// Minimal logging facade. Phase 1 will swap the default sink for spdlog;
/// for now we expose a stable API so the rest of the v3 tree can depend on
/// `core::log()` without touching `eventlog()` from the legacy tree.

#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace pvpgn::core {

enum class LogLevel : int {
    Trace    = 0,
    Debug    = 1,
    Info     = 2,
    Warn     = 3,
    Error    = 4,
    Critical = 5,
    Off      = 6,
};

constexpr std::string_view to_string(LogLevel l) noexcept {
    switch (l) {
        case LogLevel::Trace:    return "trace";
        case LogLevel::Debug:    return "debug";
        case LogLevel::Info:     return "info";
        case LogLevel::Warn:     return "warn";
        case LogLevel::Error:    return "error";
        case LogLevel::Critical: return "critical";
        case LogLevel::Off:      return "off";
    }
    return "?";
}

class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void log(LogLevel level, std::string_view module,
                     std::string_view message) noexcept = 0;
    virtual LogLevel level() const noexcept = 0;
    virtual void     set_level(LogLevel) noexcept = 0;
};

/// Swallows everything. Used in tests by default.
class NullLogger final : public ILogger {
public:
    void log(LogLevel, std::string_view, std::string_view) noexcept override {}
    LogLevel level() const noexcept override { return LogLevel::Off; }
    void set_level(LogLevel) noexcept override {}
};

/// Thread-safe writer to a `std::ostream`. Format: `level [module] message`.
class StreamLogger final : public ILogger {
public:
    explicit StreamLogger(std::ostream& out, LogLevel min = LogLevel::Info)
        : out_(&out), level_(min) {}

    void log(LogLevel level, std::string_view module,
             std::string_view message) noexcept override {
        if (static_cast<int>(level) < static_cast<int>(level_)) return;
        try {
            std::lock_guard<std::mutex> lk(mu_);
            (*out_) << to_string(level) << " [" << module << "] " << message << '\n';
        } catch (...) {
            // Logging must not throw.
        }
    }

    LogLevel level() const noexcept override { return level_; }
    void set_level(LogLevel l) noexcept override { level_ = l; }

private:
    std::ostream* out_;
    LogLevel      level_;
    std::mutex    mu_;
};

/// Process-wide default logger. Replaceable; thread-safe to read.
ILogger& default_logger() noexcept;
void     set_default_logger(std::shared_ptr<ILogger> logger) noexcept;

/// Convenience entry points. Phase 1 adds fmt-style formatting wrappers
/// once spdlog is integrated.
inline void log(LogLevel level, std::string_view module,
                std::string_view message) noexcept {
    default_logger().log(level, module, message);
}

}  // namespace pvpgn::core
