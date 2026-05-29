// SPDX-License-Identifier: GPL-2.0-or-later
#include "runtime/service_metrics.hpp"

#include <sstream>
#include <iomanip>

namespace pvpgn::runtime {

ServiceMetrics::ServiceMetrics(std::string_view service_name)
    : service_name_(service_name)
{
}

void ServiceMetrics::record_connection_accepted()
{
    connections_accepted_.fetch_add(1, std::memory_order_relaxed);
    connections_active_.fetch_add(1, std::memory_order_relaxed);
}

void ServiceMetrics::record_connection_closed()
{
    connections_active_.fetch_sub(1, std::memory_order_relaxed);
}

void ServiceMetrics::record_connection_error()
{
    connections_errors_.fetch_add(1, std::memory_order_relaxed);
}

void ServiceMetrics::record_request_received()
{
    requests_received_.fetch_add(1, std::memory_order_relaxed);
}

void ServiceMetrics::record_request_completed(std::chrono::microseconds duration)
{
    requests_completed_.fetch_add(1, std::memory_order_relaxed);
    total_request_duration_us_.fetch_add(duration.count(), std::memory_order_relaxed);
}

void ServiceMetrics::record_request_error()
{
    requests_errors_.fetch_add(1, std::memory_order_relaxed);
}

void ServiceMetrics::set_gauge(std::string_view name, double value)
{
    std::lock_guard<std::mutex> lock(custom_metrics_mutex_);
    gauges_[std::string(name)] = value;
}

void ServiceMetrics::increment_counter(std::string_view name, double delta)
{
    std::lock_guard<std::mutex> lock(custom_metrics_mutex_);
    counters_[std::string(name)] += delta;
}

double ServiceMetrics::average_request_duration_us() const
{
    uint64_t completed = requests_completed_.load(std::memory_order_relaxed);
    if (completed == 0) {
        return 0.0;
    }
    
    uint64_t total_duration = total_request_duration_us_.load(std::memory_order_relaxed);
    return static_cast<double>(total_duration) / static_cast<double>(completed);
}

std::string ServiceMetrics::export_prometheus() const
{
    std::ostringstream oss;
    
    // Service name as label
    std::string service_label = "service=\"" + service_name_ + "\"";
    
    // Connection metrics
    oss << "# HELP pvpgn_connections_accepted_total Total connections accepted\n";
    oss << "# TYPE pvpgn_connections_accepted_total counter\n";
    oss << "pvpgn_connections_accepted_total{" << service_label << "} "
        << connections_accepted_.load(std::memory_order_relaxed) << "\n";
    
    oss << "# HELP pvpgn_connections_active Active connections\n";
    oss << "# TYPE pvpgn_connections_active gauge\n";
    oss << "pvpgn_connections_active{" << service_label << "} "
        << connections_active_.load(std::memory_order_relaxed) << "\n";
    
    oss << "# HELP pvpgn_connections_errors_total Connection errors\n";
    oss << "# TYPE pvpgn_connections_errors_total counter\n";
    oss << "pvpgn_connections_errors_total{" << service_label << "} "
        << connections_errors_.load(std::memory_order_relaxed) << "\n";
    
    // Request metrics
    oss << "# HELP pvpgn_requests_received_total Total requests received\n";
    oss << "# TYPE pvpgn_requests_received_total counter\n";
    oss << "pvpgn_requests_received_total{" << service_label << "} "
        << requests_received_.load(std::memory_order_relaxed) << "\n";
    
    oss << "# HELP pvpgn_requests_completed_total Total requests completed\n";
    oss << "# TYPE pvpgn_requests_completed_total counter\n";
    oss << "pvpgn_requests_completed_total{" << service_label << "} "
        << requests_completed_.load(std::memory_order_relaxed) << "\n";
    
    oss << "# HELP pvpgn_requests_errors_total Request errors\n";
    oss << "# TYPE pvpgn_requests_errors_total counter\n";
    oss << "pvpgn_requests_errors_total{" << service_label << "} "
        << requests_errors_.load(std::memory_order_relaxed) << "\n";
    
    oss << "# HELP pvpgn_request_duration_us_total Total request duration in microseconds\n";
    oss << "# TYPE pvpgn_request_duration_us_total counter\n";
    oss << "pvpgn_request_duration_us_total{" << service_label << "} "
        << total_request_duration_us_.load(std::memory_order_relaxed) << "\n";
    
    oss << "# HELP pvpgn_request_duration_us_avg Average request duration in microseconds\n";
    oss << "# TYPE pvpgn_request_duration_us_avg gauge\n";
    oss << std::fixed << std::setprecision(2);
    oss << "pvpgn_request_duration_us_avg{" << service_label << "} "
        << average_request_duration_us() << "\n";
    
    // Custom gauges
    {
        std::lock_guard<std::mutex> lock(custom_metrics_mutex_);
        
        if (!gauges_.empty()) {
            oss << "# HELP pvpgn_custom_gauge Custom gauge metrics\n";
            oss << "# TYPE pvpgn_custom_gauge gauge\n";
            for (const auto& [name, value] : gauges_) {
                oss << "pvpgn_custom_gauge{" << service_label << ",name=\"" << name << "\"} "
                    << value << "\n";
            }
        }
        
        if (!counters_.empty()) {
            oss << "# HELP pvpgn_custom_counter Custom counter metrics\n";
            oss << "# TYPE pvpgn_custom_counter counter\n";
            for (const auto& [name, value] : counters_) {
                oss << "pvpgn_custom_counter{" << service_label << ",name=\"" << name << "\"} "
                    << value << "\n";
            }
        }
    }
    
    return oss.str();
}

} // namespace pvpgn::runtime
