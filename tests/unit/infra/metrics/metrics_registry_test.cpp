// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "infra/metrics/in_memory_metrics_registry.hpp"

namespace pvpgn::infra::metrics {

class InMemoryMetricsRegistryTest : public ::testing::Test {
protected:
    InMemoryMetricsRegistry registry_;
};

TEST_F(InMemoryMetricsRegistryTest, CounterIncrement) {
    auto counter = registry_.counter("test_counter", "A test counter");
    ASSERT_EQ(0.0, counter->value());

    counter->increment(1.0);
    EXPECT_EQ(1.0, counter->value());

    counter->increment(2.5);
    EXPECT_EQ(3.5, counter->value());

    counter->increment();  // Default is 1.0
    EXPECT_EQ(4.5, counter->value());
}

TEST_F(InMemoryMetricsRegistryTest, CounterIdempotent) {
    auto counter1 = registry_.counter("same_counter", "Help text 1");
    auto counter2 = registry_.counter("same_counter", "Help text 2");

    EXPECT_EQ(counter1.get(), counter2.get());

    counter1->increment(5.0);
    EXPECT_EQ(5.0, counter2->value());
}

TEST_F(InMemoryMetricsRegistryTest, GaugeOperations) {
    auto gauge = registry_.gauge("test_gauge", "A test gauge");
    ASSERT_EQ(0.0, gauge->value());

    gauge->set(42.0);
    EXPECT_EQ(42.0, gauge->value());

    gauge->increment(8.0);
    EXPECT_EQ(50.0, gauge->value());

    gauge->decrement(10.0);
    EXPECT_EQ(40.0, gauge->value());
}

TEST_F(InMemoryMetricsRegistryTest, HistogramObserve) {
    auto histogram = registry_.histogram("test_histogram", "A test histogram",
                                         std::vector<double>{0.1, 0.5, 1.0});

    histogram->observe(0.05);
    histogram->observe(0.25);
    histogram->observe(0.75);
    histogram->observe(1.5);

    // Note: InMemoryHistogram stores cumulative counts, so each observation
    // increments all buckets >= the value
    auto hist_impl = std::static_pointer_cast<InMemoryHistogram>(histogram);
    EXPECT_EQ(4, hist_impl->count());
    EXPECT_NEAR(2.6, hist_impl->sum(), 0.001);
}

TEST_F(InMemoryMetricsRegistryTest, PrometheusFormat) {
    auto counter = registry_.counter("pvpgn_test_counter_total", "Test counter",
                                     application::ports::MetricLabels{});
    counter->increment(42.0);

    auto gauge = registry_.gauge("pvpgn_test_gauge", "Test gauge");
    gauge->set(3.14);

    std::string output = registry_.serialize();

    // Check that HELP and TYPE lines are present
    EXPECT_NE(std::string::npos, output.find("# HELP pvpgn_test_counter_total Test counter"));
    EXPECT_NE(std::string::npos, output.find("# TYPE pvpgn_test_counter_total counter"));
    EXPECT_NE(std::string::npos, output.find("pvpgn_test_counter_total 42"));

    EXPECT_NE(std::string::npos, output.find("# HELP pvpgn_test_gauge Test gauge"));
    EXPECT_NE(std::string::npos, output.find("# TYPE pvpgn_test_gauge gauge"));
    EXPECT_NE(std::string::npos, output.find("pvpgn_test_gauge 3.14"));
}

TEST_F(InMemoryMetricsRegistryTest, MetricWithLabels) {
    application::ports::MetricLabels labels;
    labels.pairs.push_back({"protocol", "bnet"});

    auto gauge = registry_.gauge("pvpgn_connections_by_protocol",
                                 "Connections by protocol", labels);
    gauge->set(42.0);

    std::string output = registry_.serialize();

    // Check that labels are present in the output
    EXPECT_NE(std::string::npos, output.find("pvpgn_connections_by_protocol{protocol=\"bnet\"} 42"));
}

TEST_F(InMemoryMetricsRegistryTest, HistogramPrometheusFormat) {
    std::vector<double> buckets = {0.1, 0.5, 1.0};
    auto histogram = registry_.histogram("pvpgn_request_latency_ms",
                                         "Request latency in milliseconds", buckets);

    histogram->observe(0.05);
    histogram->observe(0.3);
    histogram->observe(0.8);

    std::string output = registry_.serialize();

    // Check histogram-specific suffixes
    EXPECT_NE(std::string::npos, output.find("pvpgn_request_latency_ms_bucket"));
    EXPECT_NE(std::string::npos, output.find("pvpgn_request_latency_ms_sum"));
    EXPECT_NE(std::string::npos, output.find("pvpgn_request_latency_ms_count"));
}

TEST_F(InMemoryMetricsRegistryTest, EmptyRegistry) {
    InMemoryMetricsRegistry empty_registry;
    std::string output = empty_registry.serialize();

    // Empty registry should produce empty string
    EXPECT_TRUE(output.empty());
}

}  // namespace pvpgn::infra::metrics
