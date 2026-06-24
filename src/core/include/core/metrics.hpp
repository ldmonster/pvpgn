// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file metrics.hpp
/// Core metrics interfaces and process-wide default registry facade.
///
/// This header lives in `core/` (the lowest layer) and defines lightweight
/// metric primitive interfaces that have no external dependencies.
///
/// The concrete implementation lives in `infra/metrics/`.
/// The application-layer port lives in `application/ports/metrics_registry.hpp`
/// and uses the same interface names (ICounter, IGauge, IHistogram,
/// IMetricsRegistry) in its own namespace.
///
/// Metric naming convention (Prometheus):
///   pvpgn_<context>_<noun>_<unit>
///
/// Mandatory per-use-case metrics:
///   pvpgn_<ctx>_requests_total{op,result}   — ICounter
///   pvpgn_<ctx>_request_seconds{op}         — IHistogram
///   pvpgn_<ctx>_inflight{op}                — IGauge
///
/// Usage (via the process-wide default registry):
/// @code
///   #include "core/metrics.hpp"
///
///   auto& reg = pvpgn::core::default_metrics();
///   auto logins = reg.counter("pvpgn_auth_requests_total",
///                             "Total authentication requests");
///   logins->increment();
/// @endcode

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace pvpgn::core {

// ---------------------------------------------------------------------------
// Label map
// ---------------------------------------------------------------------------

/// Ordered label map: {"op" → "login", "result" → "ok"}.
using MetricLabels = std::map<std::string, std::string>;

// ---------------------------------------------------------------------------
// Metric primitives
// ---------------------------------------------------------------------------

/// Monotonically increasing counter.
class ICounter {
public:
    virtual ~ICounter() = default;
    virtual void   increment(double amount = 1.0) = 0;
    virtual double value() const = 0;
};

/// Current value that can go up or down (e.g. active connections).
class IGauge {
public:
    virtual ~IGauge() = default;
    virtual void   set(double value) = 0;
    virtual void   increment(double amount = 1.0) = 0;
    virtual void   decrement(double amount = 1.0) = 0;
    virtual double value() const = 0;
};

/// Distribution of observed values (latency, sizes, …).
class IHistogram {
public:
    virtual ~IHistogram() = default;
    virtual void observe(double value) = 0;
};

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------

/// Process-wide metrics registry.
///
/// All `counter()`, `gauge()`, and `histogram()` calls are idempotent for the
/// same (name, labels) pair — they return the same underlying metric object.
///
/// `serialize()` returns the Prometheus text exposition format (0.0.4).
class IMetricsRegistry {
public:
    virtual ~IMetricsRegistry() = default;

    /// Register (or retrieve) a counter.
    virtual std::shared_ptr<ICounter> counter(
        std::string_view name,
        std::string_view help,
        MetricLabels     labels = {}) = 0;

    /// Register (or retrieve) a gauge.
    virtual std::shared_ptr<IGauge> gauge(
        std::string_view name,
        std::string_view help,
        MetricLabels     labels = {}) = 0;

    /// Register (or retrieve) a histogram.
    /// @param buckets  Upper bounds for each bucket (excluding +Inf).
    ///                 Defaults to standard Prometheus latency buckets when empty.
    virtual std::shared_ptr<IHistogram> histogram(
        std::string_view    name,
        std::string_view    help,
        std::vector<double> buckets = {},
        MetricLabels        labels  = {}) = 0;

    /// Serialize all registered metrics in Prometheus text format.
    virtual std::string serialize() const = 0;
};

// ---------------------------------------------------------------------------
// No-op implementations (used before composition root wires a real registry)
// ---------------------------------------------------------------------------

/// Counter that discards all increments.
class NullCounter final : public ICounter {
public:
    void   increment(double) override {}
    double value() const override { return 0.0; }
};

/// Gauge that discards all writes.
class NullGauge final : public IGauge {
public:
    void   set(double) override {}
    void   increment(double) override {}
    void   decrement(double) override {}
    double value() const override { return 0.0; }
};

/// Histogram that discards all observations.
class NullHistogram final : public IHistogram {
public:
    void observe(double) override {}
};

/// Registry that returns null metrics and serializes to an empty string.
class NullMetricsRegistry final : public IMetricsRegistry {
public:
    std::shared_ptr<ICounter> counter(
        std::string_view, std::string_view, MetricLabels) override {
        return std::make_shared<NullCounter>();
    }
    std::shared_ptr<IGauge> gauge(
        std::string_view, std::string_view, MetricLabels) override {
        return std::make_shared<NullGauge>();
    }
    std::shared_ptr<IHistogram> histogram(
        std::string_view, std::string_view,
        std::vector<double>, MetricLabels) override {
        return std::make_shared<NullHistogram>();
    }
    std::string serialize() const override { return {}; }
};

// ---------------------------------------------------------------------------
// Process-wide default registry
// ---------------------------------------------------------------------------

/// Returns the process-wide default metrics registry.
/// Thread-safe to read; never returns null (falls back to NullMetricsRegistry).
IMetricsRegistry& default_metrics() noexcept;

/// Replace the process-wide default metrics registry.
/// Typically called once from the composition root at startup.
void set_default_metrics(std::shared_ptr<IMetricsRegistry> registry) noexcept;

// ---------------------------------------------------------------------------
// Convenience free functions
// ---------------------------------------------------------------------------

inline std::shared_ptr<ICounter>
make_counter(std::string_view name, std::string_view help,
             MetricLabels labels = {}) {
    return default_metrics().counter(name, help, std::move(labels));
}

inline std::shared_ptr<IGauge>
make_gauge(std::string_view name, std::string_view help,
           MetricLabels labels = {}) {
    return default_metrics().gauge(name, help, std::move(labels));
}

inline std::shared_ptr<IHistogram>
make_histogram(std::string_view name, std::string_view help,
               std::vector<double> buckets = {},
               MetricLabels labels = {}) {
    return default_metrics().histogram(name, help, std::move(buckets),
                                       std::move(labels));
}

}  // namespace pvpgn::core
