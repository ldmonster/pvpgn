// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file otlp_trace_sink.hpp
/// ITraceSink adapter that exports completed spans via OTLP/HTTP (JSON).
///
/// Conditionally compiled: only available when PVPGN_V3_WITH_OTLP is defined
/// at CMake configure time.  When the option is OFF the header still exists
/// but the class body is replaced by a static_assert so that accidental use
/// produces a clear compile-time error rather than a linker failure.
///
/// Wire-up (composition root):
/// @code
///   #ifdef PVPGN_V3_WITH_OTLP
///   auto sink = std::make_shared<infra::tracing::OtlpTraceSink>(
///       "http://localhost:4318/v1/traces");
///   core::trace::set_global_span_sink(
///       [sink](const core::trace::Span& s){ sink->record(s); });
///   #endif
/// @endcode
///
/// The sink serialises each span to a minimal OTLP/HTTP JSON payload and
/// POSTs it synchronously (blocking the calling thread for the duration of
/// the HTTP round-trip).  For production use, wrap in an async queue.

#ifdef PVPGN_V3_WITH_OTLP

#include <string>

#include "domain/shared/ports/trace_sink.hpp"

namespace pvpgn::infra::tracing {

/// Exports spans to an OpenTelemetry collector via OTLP/HTTP (JSON).
///
/// Thread-safe: each `record()` call serialises independently.
/// The HTTP POST is performed synchronously on the calling thread.
class OtlpTraceSink final : public application::ports::ITraceSink {
public:
    /// @param endpoint  Full URL of the OTLP/HTTP traces endpoint,
    ///                  e.g. "http://localhost:4318/v1/traces".
    explicit OtlpTraceSink(std::string endpoint);
    ~OtlpTraceSink() override;

    OtlpTraceSink(const OtlpTraceSink&) = delete;
    OtlpTraceSink& operator=(const OtlpTraceSink&) = delete;

    /// Serialise the span to OTLP JSON and POST it to the configured endpoint.
    /// Errors are logged at WARN level and silently swallowed (never throws).
    void record(const ::pvpgn::core::trace::Span& span) noexcept override;

private:
    std::string endpoint_;
};

}  // namespace pvpgn::infra::tracing

#else  // PVPGN_V3_WITH_OTLP not defined

// OtlpTraceSink is NOT available in this build.
// Guard all usage with:
//   #ifdef PVPGN_V3_WITH_OTLP
//   ...
//   #endif

#endif  // PVPGN_V3_WITH_OTLP
