// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file trace.hpp
/// Lightweight distributed-tracing primitives for the v3 tree.
///
/// Provides a RAII `Span` type that records start/end times and a set of
/// string attributes.  A global `SpanSink` function is called in the Span
/// destructor so that the rest of the codebase does not need to know about
/// the concrete exporter (OTLP, log, no-op).
///
/// Usage:
/// @code
///   PVPGN_SPAN("JoinChannel");          // RAII — ends when scope exits
///   _pvpgn_span_42.set_attribute("channel", name);
/// @endcode

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace pvpgn::core::trace {

// ---------------------------------------------------------------------------
// SpanContext — propagation identifiers
// ---------------------------------------------------------------------------

/// Propagation context for a single span.
struct SpanContext {
    std::string trace_id;       ///< 16-byte hex string (32 chars)
    std::string span_id;        ///< 8-byte hex string (16 chars)
    std::string parent_span_id; ///< empty if this is a root span
    /// Whether this trace is sampled. Decided once at the root and inherited by
    /// every child (consistent sampling), so the SpanSink only fires for spans
    /// belonging to a sampled trace.
    bool        sampled = true;
};

// ---------------------------------------------------------------------------
// Span — RAII tracing unit
// ---------------------------------------------------------------------------

/// A single unit of work in a distributed trace.
///
/// - Constructor records the start time and generates random IDs.
/// - Destructor records the end time and calls the global SpanSink.
/// - Move-only; copying is deleted.
class Span {
public:
    /// Create a new root span with the given operation name. The sampling
    /// decision is made here from the process-wide sample ratio.
    explicit Span(std::string_view name);

    /// Create a child span of `parent`: it shares the parent's `trace_id` and
    /// sampling decision, sets `parent_span_id` to the parent's span, and gets
    /// a fresh `span_id`. Use this to propagate a trace across calls (and, with
    /// a remote `SpanContext`, across services).
    Span(std::string_view name, const SpanContext& parent);

    /// Records end time and calls the global SpanSink (if set).
    ~Span();

    Span(const Span&) = delete;
    Span& operator=(const Span&) = delete;

    Span(Span&&) noexcept;
    Span& operator=(Span&&) noexcept;

    /// Attach a string attribute to this span (e.g. "channel", "user").
    void set_attribute(std::string_view key, std::string_view value);

    /// Mark the span as successfully completed.
    void set_status_ok();

    /// Mark the span as failed with an error message.
    void set_status_error(std::string_view message);

    /// Access the propagation context (trace_id, span_id, parent_span_id).
    [[nodiscard]] const SpanContext& context() const noexcept;

    /// Operation name supplied at construction.
    [[nodiscard]] std::string_view name() const noexcept;

    /// Wall-clock start time.
    [[nodiscard]] std::chrono::system_clock::time_point start_time() const noexcept;

    /// Wall-clock end time (set in destructor; zero until then).
    [[nodiscard]] std::chrono::system_clock::time_point end_time() const noexcept;

    /// True if set_status_error() was called.
    [[nodiscard]] bool is_error() const noexcept;

    /// Error message (empty unless set_status_error() was called).
    [[nodiscard]] std::string_view error_message() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ---------------------------------------------------------------------------
// Global SpanSink
// ---------------------------------------------------------------------------

/// Callback type invoked in Span::~Span() with the completed span.
/// The default sink is a no-op; replace it in tests or with an OTLP adapter.
using SpanSink = std::function<void(const Span&)>;

/// Replace the process-wide span sink.  Thread-safe (uses an internal mutex).
void set_global_span_sink(SpanSink sink);

/// Retrieve the current process-wide span sink (may be a no-op).
SpanSink get_global_span_sink();

// ---------------------------------------------------------------------------
// Sampling
// ---------------------------------------------------------------------------

/// Set the process-wide head sampling ratio in [0, 1] (values are clamped).
/// 1.0 = sample every trace (the default; preserves "matches today" behaviour),
/// 0.0 = sample none. Root spans draw their sampled decision from this ratio;
/// children inherit it. Thread-safe.
void set_sample_ratio(double ratio);

/// The current process-wide sample ratio.
[[nodiscard]] double get_sample_ratio();

}  // namespace pvpgn::core::trace

// ---------------------------------------------------------------------------
// Convenience macro
// ---------------------------------------------------------------------------

/// Create a RAII Span named `name` that lives until the enclosing scope ends.
/// The variable name is mangled with __LINE__ to avoid collisions when the
/// macro is used multiple times in the same function.
#define PVPGN_SPAN(name) \
    ::pvpgn::core::trace::Span _pvpgn_span_##__LINE__{name}
