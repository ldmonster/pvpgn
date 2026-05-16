// SPDX-License-Identifier: GPL-2.0-or-later
#include "runtime/health_check.hpp"

#include <sstream>
#include <iomanip>
#include <algorithm>

namespace pvpgn::runtime {

void HealthCheckRegistry::register_check(std::string name, HealthCheckFn fn)
{
    std::lock_guard<std::mutex> lock(mutex_);
    checks_[std::move(name)] = std::move(fn);
}

void HealthCheckRegistry::unregister_check(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    checks_.erase(name);
}

std::unordered_map<std::string, HealthCheckResult> HealthCheckRegistry::run_all() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::unordered_map<std::string, HealthCheckResult> results;
    
    for (const auto& [name, check_fn] : checks_) {
        try {
            results[name] = check_fn();
        } catch (const std::exception& e) {
            HealthCheckResult result;
            result.status = HealthStatus::unhealthy;
            result.message = std::string("Check threw exception: ") + e.what();
            result.checked_at = std::chrono::system_clock::now();
            results[name] = result;
        } catch (...) {
            HealthCheckResult result;
            result.status = HealthStatus::unhealthy;
            result.message = "Check threw unknown exception";
            result.checked_at = std::chrono::system_clock::now();
            results[name] = result;
        }
    }
    
    return results;
}

HealthStatus HealthCheckRegistry::overall_status() const
{
    auto results = run_all();
    
    HealthStatus worst = HealthStatus::healthy;
    
    for (const auto& [name, result] : results) {
        if (result.status == HealthStatus::unhealthy) {
            worst = HealthStatus::unhealthy;
            break;
        } else if (result.status == HealthStatus::degraded && worst != HealthStatus::unhealthy) {
            worst = HealthStatus::degraded;
        }
    }
    
    return worst;
}

std::string HealthCheckRegistry::export_json() const
{
    auto results = run_all();
    
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"overall_status\": \"";
    
    HealthStatus overall = overall_status();
    switch (overall) {
        case HealthStatus::healthy:
            oss << "healthy";
            break;
        case HealthStatus::degraded:
            oss << "degraded";
            break;
        case HealthStatus::unhealthy:
            oss << "unhealthy";
            break;
    }
    
    oss << "\",\n";
    oss << "  \"checks\": {\n";
    
    bool first = true;
    for (const auto& [name, result] : results) {
        if (!first) {
            oss << ",\n";
        }
        first = false;
        
        oss << "    \"" << name << "\": {\n";
        oss << "      \"status\": \"";
        
        switch (result.status) {
            case HealthStatus::healthy:
                oss << "healthy";
                break;
            case HealthStatus::degraded:
                oss << "degraded";
                break;
            case HealthStatus::unhealthy:
                oss << "unhealthy";
                break;
        }
        
        oss << "\",\n";
        oss << "      \"message\": \"" << result.message << "\",\n";
        
        auto time_t_val = std::chrono::system_clock::to_time_t(result.checked_at);
        oss << "      \"checked_at\": " << time_t_val << ",\n";
        
        oss << "      \"details\": {\n";
        bool first_detail = true;
        for (const auto& [key, value] : result.details) {
            if (!first_detail) {
                oss << ",\n";
            }
            first_detail = false;
            oss << "        \"" << key << "\": \"" << value << "\"";
        }
        oss << "\n      }\n";
        oss << "    }";
    }
    
    oss << "\n  }\n";
    oss << "}\n";
    
    return oss.str();
}

std::string HealthCheckRegistry::export_prometheus() const
{
    auto results = run_all();
    
    std::ostringstream oss;
    
    // Overall status metric
    oss << "# HELP pvpgn_health_status Overall health status (0=healthy, 1=degraded, 2=unhealthy)\n";
    oss << "# TYPE pvpgn_health_status gauge\n";
    
    HealthStatus overall = overall_status();
    int status_value = 0;
    switch (overall) {
        case HealthStatus::healthy:
            status_value = 0;
            break;
        case HealthStatus::degraded:
            status_value = 1;
            break;
        case HealthStatus::unhealthy:
            status_value = 2;
            break;
    }
    
    oss << "pvpgn_health_status " << status_value << "\n";
    
    // Individual check status metrics
    oss << "# HELP pvpgn_health_check_status Individual health check status (0=healthy, 1=degraded, 2=unhealthy)\n";
    oss << "# TYPE pvpgn_health_check_status gauge\n";
    
    for (const auto& [name, result] : results) {
        int check_status = 0;
        switch (result.status) {
            case HealthStatus::healthy:
                check_status = 0;
                break;
            case HealthStatus::degraded:
                check_status = 1;
                break;
            case HealthStatus::unhealthy:
                check_status = 2;
                break;
        }
        
        oss << "pvpgn_health_check_status{check=\"" << name << "\"} " << check_status << "\n";
    }
    
    return oss.str();
}

} // namespace pvpgn::runtime
