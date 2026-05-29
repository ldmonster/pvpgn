// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file log_trace_sink.hpp
/// ITraceSink adapter that writes completed spans to the pvpgn structured logger.
///
/// Each completed span is emitted as a single INFO-level log line with the
/// following fields:
///   trace_id, span_id, parent_span_id, name, duration_us, status
///
/// This sink is always available (no optional dependency).  It is the default
/// sink used in development and CI builds where an OTLP collector is not
/// running.
///
/// Usage (composition root):
/// @code
///   auto sink = std::make_shared<infra::tracing::LogTraceSink>();
///   core::trace::set_global_span_sink(
///       [sink](const core::trace::Span& s){ sink->record(s); });
/// @endcode

#include "application/ports/trace_sink.hpp"

namespace pvpgn::infra::tracing {

/// Writes each completed span as a structured log line.
class LogTraceSink final : public application::ports::ITraceSink {
public:
    LogTraceSink() = default;
    ~LogTraceSink() override = default;

    LogTraceSink(const LogTraceSink&) = delete;
    LogTraceSink& operator=(const LogTraceSink&) = delete;

    /// Emit the span as a structured INFO log line.
    /// Thread-safe; never throws.
    void record(const ::pvpgn::core::trace::Span& span) noexcept override;
};

}  // namespace pvpgn::infra::tracing
