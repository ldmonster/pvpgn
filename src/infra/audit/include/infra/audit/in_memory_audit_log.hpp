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

#include "domain/moderation/ports.hpp"

namespace pvpgn::infra::audit {

class InMemoryAuditLog : public domain::moderation::IAuditLog {
public:
    explicit InMemoryAuditLog(std::size_t max_entries = 1000)
        : max_entries_(max_entries) {}

    void record(const domain::moderation::AuditEntry& entry) override;

    std::vector<domain::moderation::AuditEntry> recent(std::size_t count) const override;

private:
    std::size_t max_entries_;
    mutable std::mutex mutex_;
    std::deque<domain::moderation::AuditEntry> entries_;
};

}  // namespace pvpgn::infra::audit
