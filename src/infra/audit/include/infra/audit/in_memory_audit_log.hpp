// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file in_memory_audit_log.hpp
/// In-memory implementation of IAuditLog.
///
/// Uses a circular buffer to store the last N audit entries.
/// Thread-safe with mutex protection.

#include <deque>
#include <mutex>
#include <vector>

#include "application/ports/audit_log.hpp"

namespace pvpgn::infra::audit {

class InMemoryAuditLog : public application::ports::IAuditLog {
public:
    explicit InMemoryAuditLog(std::size_t max_entries = 1000)
        : max_entries_(max_entries) {}

    void record(const application::ports::AuditEntry& entry) override;

    std::vector<application::ports::AuditEntry> recent(std::size_t count) const override;

private:
    std::size_t max_entries_;
    mutable std::mutex mutex_;
    std::deque<application::ports::AuditEntry> entries_;
};

}  // namespace pvpgn::infra::audit
