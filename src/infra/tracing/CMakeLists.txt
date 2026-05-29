# Distributed tracing adapters for the v3 tree.
#
# Provides:
#   - LogTraceSink  — always built; writes spans to the spdlog logger
#   - OtlpTraceSink — built only when PVPGN_V3_WITH_OTLP=ON;
#                     exports spans via OTLP/HTTP (JSON) to an OTel collector
#
# Usage in the composition root:
#   auto sink = std::make_shared<infra::tracing::LogTraceSink>();
#   core::trace::set_global_span_sink(
#       [sink](const core::trace::Span& s){ sink->record(s); });

option(PVPGN_V3_WITH_OTLP
    "Build the OTLP/HTTP trace exporter (requires Boost.Beast)"
    OFF)

# Base sources — always compiled.
set(_tracing_sources
    src/log_trace_sink.cpp
)

# Optional OTLP source.
if(PVPGN_V3_WITH_OTLP)
    list(APPEND _tracing_sources src/otlp_trace_sink.cpp)
endif()

pvpgn_v3_add_library(infra_tracing
    STATIC
    SOURCES
        ${_tracing_sources}
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
        application_ports
    DEPS
        spdlog::spdlog
)

# Propagate the OTLP compile-time flag so that both the library and any
# consumer that includes otlp_trace_sink.hpp see the same definition.
if(PVPGN_V3_WITH_OTLP)
    target_compile_definitions(infra_tracing PUBLIC PVPGN_V3_WITH_OTLP)
    # Boost.Beast is header-only but needs Boost::system at link time.
    target_link_libraries(infra_tracing PRIVATE Boost::system)
    message(STATUS "pvpgn v3: OTLP trace exporter enabled")
else()
    message(STATUS "pvpgn v3: OTLP trace exporter disabled (PVPGN_V3_WITH_OTLP=OFF)")
endif()
