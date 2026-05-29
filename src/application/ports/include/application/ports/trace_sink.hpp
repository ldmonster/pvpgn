// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file trace_sink.hpp
/// Port interface for receiving completed trace spans.
///
/// Adapters (OTLP exporter, log sink, no-op) implement this interface.
/// The composition root wires the chosen adapter to the global SpanSink
/// via `core::trace::set_global_span_sink()`.

namespace pvpgn::core::trace {
class Span;
}  // namespace pvpgn::core::trace

namespace pvpgn::application::ports {

/// Port: receives a completed `Span` for export/storage.
class ITraceSink {
public:
    virtual ~ITraceSink() = default;

    /// Called once per completed span (from the Span destructor thread).
    /// Implementations MUST be thread-safe and MUST NOT throw.
    virtual void record(const ::pvpgn::core::trace::Span& span) noexcept = 0;
};

}  // namespace pvpgn::application::ports
