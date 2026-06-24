// SPDX-License-Identifier: GPL-2.0-or-later

/// @file metrics.cpp
/// Process-wide default metrics registry.

#include "core/metrics.hpp"

#include <memory>
#include <mutex>

namespace pvpgn::core {

namespace {

std::mutex                         g_mu;
std::shared_ptr<IMetricsRegistry>  g_registry;

}  // namespace

IMetricsRegistry& default_metrics() noexcept {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_registry) {
        // Lazy-initialize a NullMetricsRegistry so callers never get null.
        static NullMetricsRegistry s_null;
        return s_null;
    }
    return *g_registry;
}

void set_default_metrics(std::shared_ptr<IMetricsRegistry> registry) noexcept {
    std::lock_guard<std::mutex> lk(g_mu);
    g_registry = std::move(registry);
}

}  // namespace pvpgn::core
