// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file json_line_logger.hpp
/// `core::ILogger` that writes one JSON object per record, one record
/// per line (NDJSON / JSON Lines format), suitable for piping into
/// `jq`, Logstash, Vector, or any structured-log consumer (Batch 22d).
///
/// Output format (single line, terminated by `\n`):
///
///   {"ts":<unix-ms>,"lvl":"info","mod":"<module>","msg":"<message>"}
///
/// String fields are JSON-escaped (`"`, `\`, control chars). Bytes
/// outside the ASCII printable range pass through as-is (caller
/// responsibility -- the legacy log already accepts UTF-8). The
/// timestamp is taken from the injected clock (default = system
/// monotonic-since-epoch); tests use a fixed-clock fake.

#include <chrono>
#include <functional>
#include <mutex>
#include <ostream>
#include <string_view>

#include "core/logging.hpp"

namespace pvpgn::infra::log {

/// Clock function returning Unix-epoch milliseconds. Replaced in
/// tests with a fixed-value lambda.
using ClockFn = std::function<std::int64_t()>;

class JsonLineLogger final : public core::ILogger {
public:
    /// `out` must outlive the logger. `clock` defaults to system
    /// `steady_clock::now()`-derived milliseconds since UNIX epoch.
    explicit JsonLineLogger(std::ostream& out,
                            core::LogLevel min = core::LogLevel::Info,
                            ClockFn        clock = {});

    void log(core::LogLevel level,
             std::string_view module,
             std::string_view message) noexcept override;

    /// Structured-field override (Batch 27c). Each `Field` becomes
    /// a top-level JSON key in the output object, after `msg`.
    /// Keys must be JSON-safe identifiers (`[A-Za-z0-9_]`); values
    /// are JSON-string-escaped exactly like `msg`.
    void log_kv(core::LogLevel level,
                std::string_view module,
                std::string_view message,
                std::span<const Field> fields) noexcept override;

    core::LogLevel level() const noexcept override { return level_; }
    void set_level(core::LogLevel l) noexcept override { level_ = l; }

private:
    std::ostream*  out_;
    core::LogLevel level_;
    ClockFn        clock_;
    std::mutex     mu_;
};

}  // namespace pvpgn::infra::log
