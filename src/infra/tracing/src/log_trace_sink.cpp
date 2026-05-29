// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/tracing/log_trace_sink.hpp"

#include <chrono>
#include <string>

#include <spdlog/spdlog.h>

#include "core/trace.hpp"

namespace pvpgn::infra::tracing {

void LogTraceSink::record(const ::pvpgn::core::trace::Span& span) noexcept {
    try {
        const auto& ctx = span.context();

        // Compute duration in microseconds.
        const auto dur_us = std::chrono::duration_cast<std::chrono::microseconds>(
            span.end_time() - span.start_time()).count();

        if (span.is_error()) {
            SPDLOG_INFO("[trace] span={} trace={} parent={} op={} dur_us={} status=error msg={}",
                ctx.span_id,
                ctx.trace_id,
                ctx.parent_span_id.empty() ? "-" : ctx.parent_span_id,
                span.name(),
                dur_us,
                span.error_message());
        } else {
            SPDLOG_INFO("[trace] span={} trace={} parent={} op={} dur_us={} status=ok",
                ctx.span_id,
                ctx.trace_id,
                ctx.parent_span_id.empty() ? "-" : ctx.parent_span_id,
                span.name(),
                dur_us);
        }
    } catch (...) {
        // Never propagate exceptions from a trace sink.
    }
}

}  // namespace pvpgn::infra::tracing
