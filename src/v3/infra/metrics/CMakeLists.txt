# Prometheus-compatible metrics collection and HTTP exposition endpoint
# Provides:
#   - IMetricsRegistry port (counter, gauge, histogram)
#   - InMemoryMetricsRegistry implementation with Prometheus text format
#   - ServerMetrics aggregation struct
#   - HttpMetricsServer for /metrics endpoint

pvpgn_v3_add_library(infra_metrics
    STATIC
    SOURCES
        src/in_memory_metrics_registry.cpp
        src/server_metrics.cpp
        src/http_metrics_server.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
        application_ports
        infra_net
    DEPS
        Boost::system
)
