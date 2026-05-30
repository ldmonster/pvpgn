// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file trace_sink.hpp
/// Application-layer port for export of completed tracing spans.

#include "core/trace.hpp"

namespace pvpgn::application::ports {

/// Sink for completed `core::trace::Span` objects.
///
/// Implementations forward spans to a backing system (structured log,
/// OTLP exporter, …). `record()` must be noexcept and cheap; long work
/// must be dispatched off the calling thread by the implementation.
class ITraceSink {
public:
    virtual ~ITraceSink() = default;

    ITraceSink(const ITraceSink&)            = delete;
    ITraceSink& operator=(const ITraceSink&) = delete;
    ITraceSink(ITraceSink&&)                 = delete;
    ITraceSink& operator=(ITraceSink&&)      = delete;

    virtual void record(const ::pvpgn::core::trace::Span& span) noexcept = 0;

protected:
    ITraceSink() = default;
};

} // namespace pvpgn::application::ports
