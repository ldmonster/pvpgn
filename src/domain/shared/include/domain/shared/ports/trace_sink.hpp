// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file trace_sink.hpp
/// Application-layer port for a tracing sink.
///
/// An `ITraceSink` receives each completed span so it can be exported
/// (logged, shipped to an OTLP collector, etc.). The core tracing layer
/// (`core::trace`) owns the `Span` type and a global `SpanSink` hook; this
/// port lets the application/infra layers provide concrete sinks without the
/// core depending on them.

#include "core/trace.hpp"

namespace pvpgn::application::ports {

/// Sink that receives completed spans.
///
/// Implementations must be thread-safe and must not throw (the core span
/// destructor invokes the global sink and cannot propagate exceptions).
class ITraceSink {
public:
    virtual ~ITraceSink() = default;

    /// Record a single completed span.
    virtual void record(const ::pvpgn::core::trace::Span& span) noexcept = 0;
};

}  // namespace pvpgn::application::ports
