// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file metrics_registry.hpp
/// Application-layer port for the metrics subsystem (Plan 10 — Observability).
///
/// Re-exports the interfaces defined in `core/metrics.hpp` into the
/// `application::ports` namespace so that application-layer code can depend
/// on this header without knowing about the `core` layer directly.
///
/// Concrete implementation: `infra/metrics/in_memory_metrics_registry.hpp`.

#include "core/metrics.hpp"

namespace pvpgn::application::ports {

// ---------------------------------------------------------------------------
// Re-export core metric types into the application::ports namespace.
// All existing consumers of `application::ports::IMetricsRegistry` etc.
// continue to compile unchanged.
// ---------------------------------------------------------------------------

using MetricLabels    = core::MetricLabels;
using ICounter        = core::ICounter;
using IGauge          = core::IGauge;
using IHistogram      = core::IHistogram;
using IMetricsRegistry = core::IMetricsRegistry;

}  // namespace pvpgn::application::ports
