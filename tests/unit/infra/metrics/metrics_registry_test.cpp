// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "infra/metrics/in_memory_metrics_registry.hpp"

namespace pvpgn::infra::metrics {

TEST_CASE("InMemoryMetricsRegistry: CounterIncrement", "[infra][metrics]") {
    InMemoryMetricsRegistry registry;
    auto counter = registry.counter("test_counter", "A test counter");
    REQUIRE(counter->value() == 0.0);

    counter->increment(1.0);
    REQUIRE(counter->value() == 1.0);

    counter->increment(2.5);
    REQUIRE(counter->value() == 3.5);

    counter->increment();  // Default is 1.0
    REQUIRE(counter->value() == 4.5);
}

TEST_CASE("InMemoryMetricsRegistry: CounterIdempotent", "[infra][metrics]") {
    InMemoryMetricsRegistry registry;
    auto counter1 = registry.counter("same_counter", "Help text 1");
    auto counter2 = registry.counter("same_counter", "Help text 2");

    REQUIRE(counter1.get() == counter2.get());

    counter1->increment(5.0);
    REQUIRE(counter2->value() == 5.0);
}

TEST_CASE("InMemoryMetricsRegistry: GaugeOperations", "[infra][metrics]") {
    InMemoryMetricsRegistry registry;
    auto gauge = registry.gauge("test_gauge", "A test gauge");
    REQUIRE(gauge->value() == 0.0);

    gauge->set(42.0);
    REQUIRE(gauge->value() == 42.0);

    gauge->increment(8.0);
    REQUIRE(gauge->value() == 50.0);

    gauge->decrement(10.0);
    REQUIRE(gauge->value() == 40.0);
}

TEST_CASE("InMemoryMetricsRegistry: HistogramObserve", "[infra][metrics]") {
    InMemoryMetricsRegistry registry;
    auto histogram = registry.histogram("test_histogram", "A test histogram",
                                         std::vector<double>{0.1, 0.5, 1.0});

    histogram->observe(0.05);
    histogram->observe(0.25);
    histogram->observe(0.75);
    histogram->observe(1.5);

    // Note: InMemoryHistogram stores cumulative counts, so each observation
    // increments all buckets >= the value
    auto hist_impl = std::static_pointer_cast<InMemoryHistogram>(histogram);
    REQUIRE(hist_impl->count() == 4);
    REQUIRE_THAT(hist_impl->sum(), Catch::Matchers::WithinAbs(2.55, 0.01));
}

TEST_CASE("InMemoryMetricsRegistry: PrometheusFormat", "[infra][metrics]") {
    InMemoryMetricsRegistry registry;
    auto counter = registry.counter("pvpgn_test_counter_total", "Test counter",
                                     application::ports::MetricLabels{});
    counter->increment(42.0);

    auto gauge = registry.gauge("pvpgn_test_gauge", "Test gauge");
    gauge->set(3.14);

    std::string output = registry.serialize();

    // Check that HELP and TYPE lines are present
    REQUIRE(output.find("# HELP pvpgn_test_counter_total Test counter") != std::string::npos);
    REQUIRE(output.find("# TYPE pvpgn_test_counter_total counter") != std::string::npos);
    REQUIRE(output.find("pvpgn_test_counter_total 42") != std::string::npos);

    REQUIRE(output.find("# HELP pvpgn_test_gauge Test gauge") != std::string::npos);
    REQUIRE(output.find("# TYPE pvpgn_test_gauge gauge") != std::string::npos);
    REQUIRE(output.find("pvpgn_test_gauge 3.14") != std::string::npos);
}

TEST_CASE("InMemoryMetricsRegistry: MetricWithLabels", "[infra][metrics]") {
    InMemoryMetricsRegistry registry;
    application::ports::MetricLabels labels;
    labels.pairs.push_back({"protocol", "bnet"});

    auto gauge = registry.gauge("pvpgn_connections_by_protocol",
                                 "Connections by protocol", labels);
    gauge->set(42.0);

    std::string output = registry.serialize();

    // Check that labels are present in the output
    REQUIRE(output.find("pvpgn_connections_by_protocol{protocol=\"bnet\"} 42") != std::string::npos);
}

TEST_CASE("InMemoryMetricsRegistry: HistogramPrometheusFormat", "[infra][metrics]") {
    InMemoryMetricsRegistry registry;
    std::vector<double> buckets = {0.1, 0.5, 1.0};
    auto histogram = registry.histogram("pvpgn_request_latency_ms",
                                         "Request latency in milliseconds", buckets);

    histogram->observe(0.05);
    histogram->observe(0.3);
    histogram->observe(0.8);

    std::string output = registry.serialize();

    // Check histogram-specific suffixes
    REQUIRE(output.find("pvpgn_request_latency_ms_bucket") != std::string::npos);
    REQUIRE(output.find("pvpgn_request_latency_ms_sum") != std::string::npos);
    REQUIRE(output.find("pvpgn_request_latency_ms_count") != std::string::npos);
}

TEST_CASE("InMemoryMetricsRegistry: EmptyRegistry", "[infra][metrics]") {
    InMemoryMetricsRegistry empty_registry;
    std::string output = empty_registry.serialize();

    // Empty registry should produce empty string
    REQUIRE(output.empty());
}

}  // namespace pvpgn::infra::metrics
