// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file metrics_registry.hpp
/// Application-layer port for metrics registry.
/// Re-exports core metrics interfaces for use in application code.

#include "core/metrics.hpp"

namespace pvpgn::application::ports {

// Re-export core metrics interfaces
using core::ICounter;
using core::IGauge;
using core::IHistogram;
using core::IMetricsRegistry;
using core::MetricLabels;

}  // namespace pvpgn::application::ports
