// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file in_memory_metrics_registry.hpp
/// In-memory implementation of IMetricsRegistry with Prometheus text format serialization.

#include <atomic>
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#include "core/metrics.hpp"

namespace pvpgn::infra::metrics {

/// In-memory counter implementation.
class InMemoryCounter : public core::ICounter {
public:
    void increment(double amount = 1.0) override;
    double value() const override;

private:
    std::atomic<double> value_{0.0};
};

/// In-memory gauge implementation.
class InMemoryGauge : public core::IGauge {
public:
    void set(double value) override;
    void increment(double amount = 1.0) override;
    void decrement(double amount = 1.0) override;
    double value() const override;

private:
    std::atomic<double> value_{0.0};
};

/// In-memory histogram implementation.
/// Uses standard Prometheus buckets: 0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0, +Inf
class InMemoryHistogram : public core::IHistogram {
public:
    explicit InMemoryHistogram(std::vector<double> buckets = {});
    void observe(double value) override;

    // Accessors for serialization
    std::vector<double> bucket_values() const;
    double sum() const;
    std::size_t count() const;

private:
    std::vector<double> buckets_;
    std::vector<std::shared_ptr<std::atomic<std::size_t>>> bucket_counts_;
    std::atomic<std::size_t> count_{0};
    std::atomic<double> sum_{0.0};
    mutable std::shared_mutex mu_;

    static std::vector<double> default_buckets();
};

/// In-memory metrics registry with Prometheus text format serialization.
class InMemoryMetricsRegistry : public core::IMetricsRegistry {
public:
    InMemoryMetricsRegistry() = default;

    std::shared_ptr<core::ICounter> counter(
        std::string_view name,
        std::string_view help,
        core::MetricLabels labels = {}) override;

    std::shared_ptr<core::IGauge> gauge(
        std::string_view name,
        std::string_view help,
        core::MetricLabels labels = {}) override;

    std::shared_ptr<core::IHistogram> histogram(
        std::string_view name,
        std::string_view help,
        std::vector<double> buckets = {},
        core::MetricLabels labels = {}) override;

    std::string serialize() const override;

private:
    struct MetricInfo {
        std::string name;
        std::string help;
        core::MetricLabels labels;
        std::shared_ptr<void> metric;  // Holds ICounter, IGauge, or IHistogram
        enum class Type { Counter, Gauge, Histogram } type;
    };

    mutable std::shared_mutex metrics_mu_;
    std::map<std::string, MetricInfo> metrics_;

    std::string labels_str(const core::MetricLabels& labels) const;
};

}  // namespace pvpgn::infra::metrics
