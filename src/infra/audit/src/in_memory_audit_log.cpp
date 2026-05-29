// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/audit/in_memory_audit_log.hpp"

namespace pvpgn::infra::audit {

void InMemoryAuditLog::record(const application::ports::AuditEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);

    entries_.push_back(entry);

    // Maintain circular buffer size
    while (entries_.size() > max_entries_) {
        entries_.pop_front();
    }
}

std::vector<application::ports::AuditEntry> InMemoryAuditLog::recent(
    std::size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<application::ports::AuditEntry> result;
    std::size_t start_idx = entries_.size() > count ? entries_.size() - count : 0;

    for (std::size_t i = start_idx; i < entries_.size(); ++i) {
        result.push_back(entries_[i]);
    }

    return result;
}

}  // namespace pvpgn::infra::audit
