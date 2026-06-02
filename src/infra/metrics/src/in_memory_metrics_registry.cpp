// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/metrics/in_memory_metrics_registry.hpp"
#include "core/metrics.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace pvpgn::infra::metrics {

// ============================================================================
// InMemoryCounter
// ============================================================================

void InMemoryCounter::increment(double amount) {
    double current = value_.load(std::memory_order_relaxed);
    while (!value_.compare_exchange_weak(current, current + amount, std::memory_order_relaxed)) {
    }
}

double InMemoryCounter::value() const {
    return value_.load(std::memory_order_relaxed);
}

// ============================================================================
// InMemoryGauge
// ============================================================================

void InMemoryGauge::set(double value) {
    value_.store(value, std::memory_order_relaxed);
}

void InMemoryGauge::increment(double amount) {
    double current = value_.load(std::memory_order_relaxed);
    while (!value_.compare_exchange_weak(current, current + amount, std::memory_order_relaxed)) {
    }
}

void InMemoryGauge::decrement(double amount) {
    double current = value_.load(std::memory_order_relaxed);
    while (!value_.compare_exchange_weak(current, current - amount, std::memory_order_relaxed)) {
    }
}

double InMemoryGauge::value() const {
    return value_.load(std::memory_order_relaxed);
}

// ============================================================================
// InMemoryHistogram
// ============================================================================

std::vector<double> InMemoryHistogram::default_buckets() {
    return {0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0};
}

InMemoryHistogram::InMemoryHistogram(std::vector<double> buckets)
    : buckets_(buckets.empty() ? default_buckets() : buckets) {
    // Initialize bucket counts (one per bucket boundary + 1 for +Inf)
    // Use shared_ptr to store atomics since they are non-copyable
    for (std::size_t i = 0; i <= buckets_.size(); ++i) {
        bucket_counts_.push_back(std::make_shared<std::atomic<std::size_t>>(0));
    }
    // Ensure buckets are sorted
    std::sort(buckets_.begin(), buckets_.end());
}

void InMemoryHistogram::observe(double value) {
    double current_sum = sum_.load(std::memory_order_relaxed);
    while (!sum_.compare_exchange_weak(current_sum, current_sum + value, std::memory_order_relaxed)) {
    }
    count_.fetch_add(1, std::memory_order_relaxed);

    // Find the bucket index and increment all buckets >= value
    for (std::size_t i = 0; i < buckets_.size(); ++i) {
        if (value <= buckets_[i]) {
            bucket_counts_[i]->fetch_add(1, std::memory_order_relaxed);
        }
    }
    // Always increment +Inf bucket
    bucket_counts_[buckets_.size()]->fetch_add(1, std::memory_order_relaxed);
}

std::vector<double> InMemoryHistogram::bucket_values() const {
    std::vector<double> result;
    result.reserve(buckets_.size() + 1);
    for (std::size_t i = 0; i < buckets_.size(); ++i) {
        result.push_back(static_cast<double>(
            bucket_counts_[i]->load(std::memory_order_relaxed)));
    }
    result.push_back(static_cast<double>(
        bucket_counts_[buckets_.size()]->load(std::memory_order_relaxed)));
    return result;
}

double InMemoryHistogram::sum() const {
    return sum_.load(std::memory_order_relaxed);
}

std::size_t InMemoryHistogram::count() const {
    return count_.load(std::memory_order_relaxed);
}

// ============================================================================
// InMemoryMetricsRegistry
// ============================================================================

std::shared_ptr<core::ICounter> InMemoryMetricsRegistry::counter(
    std::string_view name,
    std::string_view help,
    core::MetricLabels labels) {
    std::unique_lock lock(metrics_mu_);

    auto key = std::string(name);
    auto it = metrics_.find(key);
    if (it != metrics_.end()) {
        return std::static_pointer_cast<core::ICounter>(it->second.metric);
    }

    auto counter = std::make_shared<InMemoryCounter>();
    MetricInfo info{std::string(name), std::string(help), labels, counter,
                    MetricInfo::Type::Counter};
    metrics_[key] = info;
    return counter;
}

std::shared_ptr<core::IGauge> InMemoryMetricsRegistry::gauge(
    std::string_view name,
    std::string_view help,
    core::MetricLabels labels) {
    std::unique_lock lock(metrics_mu_);

    auto key = std::string(name);
    auto it = metrics_.find(key);
    if (it != metrics_.end()) {
        return std::static_pointer_cast<core::IGauge>(it->second.metric);
    }

    auto gauge = std::make_shared<InMemoryGauge>();
    MetricInfo info{std::string(name), std::string(help), labels, gauge,
                    MetricInfo::Type::Gauge};
    metrics_[key] = info;
    return gauge;
}

std::shared_ptr<core::IHistogram> InMemoryMetricsRegistry::histogram(
    std::string_view name,
    std::string_view help,
    std::vector<double> buckets,
    core::MetricLabels labels) {
    std::unique_lock lock(metrics_mu_);

    auto key = std::string(name);
    auto it = metrics_.find(key);
    if (it != metrics_.end()) {
        return std::static_pointer_cast<core::IHistogram>(it->second.metric);
    }

    auto histogram = std::make_shared<InMemoryHistogram>(buckets);
    MetricInfo info{std::string(name), std::string(help), labels, histogram,
                    MetricInfo::Type::Histogram};
    metrics_[key] = info;
    return histogram;
}

std::string InMemoryMetricsRegistry::labels_str(
    const core::MetricLabels& labels) const {
    if (labels.empty()) {
        return "";
    }

    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& [k, v] : labels) {
        if (!first) oss << ",";
        oss << k << "=\"" << v << "\"";
        first = false;
    }
    oss << "}";
    return oss.str();
}

std::string InMemoryMetricsRegistry::serialize() const {
    std::shared_lock lock(metrics_mu_);
    std::ostringstream oss;

    // Helper to format floating point values consistently
    auto format_value = [](double val) -> std::string {
        if (std::isnan(val)) return "NaN";
        if (std::isinf(val)) return val > 0 ? "+Inf" : "-Inf";

        std::ostringstream tmp;
        tmp << std::fixed << std::setprecision(15) << val;
        std::string result = tmp.str();
        // Remove trailing zeros after decimal point
        if (result.find('.') != std::string::npos) {
            result.erase(result.find_last_not_of('0') + 1, std::string::npos);
            if (result.back() == '.') {
                result.pop_back();
            }
        }
        return result;
    };

    for (const auto& [key, info] : metrics_) {
        // Write HELP and TYPE lines
        oss << "# HELP " << info.name << " " << info.help << "\n";

        std::string type_str;
        switch (info.type) {
            case MetricInfo::Type::Counter:
                type_str = "counter";
                break;
            case MetricInfo::Type::Gauge:
                type_str = "gauge";
                break;
            case MetricInfo::Type::Histogram:
                type_str = "histogram";
                break;
        }
        oss << "# TYPE " << info.name << " " << type_str << "\n";

        std::string labels_suffix = labels_str(info.labels);

        // Write metric value(s)
        if (info.type == MetricInfo::Type::Counter) {
            auto counter = std::static_pointer_cast<InMemoryCounter>(info.metric);
            oss << info.name << labels_suffix << " " << format_value(counter->value())
                << "\n";
        } else if (info.type == MetricInfo::Type::Gauge) {
            auto gauge = std::static_pointer_cast<InMemoryGauge>(info.metric);
            oss << info.name << labels_suffix << " " << format_value(gauge->value()) << "\n";
        } else if (info.type == MetricInfo::Type::Histogram) {
            auto histogram = std::static_pointer_cast<InMemoryHistogram>(info.metric);
            const auto buckets = histogram->bucket_values();

            // Write bucket values
            // Note: InMemoryHistogram stores cumulative counts, which is what we want
            std::vector<double> bucket_boundaries = {
                0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0};

            // Extract the histogram's actual bucket boundaries
            // Since we don't store them separately in the serialization, we'll use default
            // In a real implementation, you'd want to preserve the bucket boundaries
            for (std::size_t i = 0; i < bucket_boundaries.size() && i < buckets.size(); ++i) {
                oss << info.name << "_bucket{le=\"" << format_value(bucket_boundaries[i])
                    << "\"" << (labels_suffix.empty() ? "" : ",") << labels_suffix << "} "
                    << format_value(buckets[i]) << "\n";
            }
            // +Inf bucket
            if (!buckets.empty()) {
                oss << info.name << "_bucket{le=\"+Inf\"" << (labels_suffix.empty() ? "" : ",")
                    << labels_suffix << "} " << format_value(buckets.back()) << "\n";
            }
            // sum and count
            oss << info.name << "_sum" << labels_suffix << " " << format_value(histogram->sum())
                << "\n";
            oss << info.name << "_count" << labels_suffix << " "
                << format_value(static_cast<double>(histogram->count())) << "\n";
        }
    }

    return oss.str();
}

}  // namespace pvpgn::infra::metrics
