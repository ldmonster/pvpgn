# infra/health — HTTP handlers for health probes and Prometheus metrics scrape
# (Plan 10 — Observability and Operations)
#
# Provides:
#   HealthHandler   — /healthz (liveness), /readyz (readiness), /varz (debug)
#   MetricsHandler  — /metrics (Prometheus text format)
#
# These handlers are decoupled from any specific HTTP server: they take a
# path string and return a complete HTTP/1.1 response string.  The
# HttpMetricsServer in infra/metrics/ uses them internally; they can also be
# embedded in the EmbeddedWebServer (infra/webui/).

pvpgn_v3_add_library(infra_health
    STATIC
    SOURCES
        src/health_handler.cpp
        src/metrics_handler.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
        application_ports
)
