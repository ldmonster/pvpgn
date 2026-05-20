// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file file_audit_log.hpp
/// Append-only file implementation of IAuditLog.
///
/// Each `record()` flushes a single text line. Format (TSV):
///   <iso8601_utc>\t<action_name>\t<actor_id>\t<subject>\t<details>
/// `subject` and `details` have tabs and newlines escaped via
/// backslash-escaping so the line is grep-able and round-trippable.
///
/// Concurrency: a mutex serializes writes. The file handle is held
/// open for the lifetime of the object; `record()` calls `flush` so
/// crashes lose at most the in-flight line.
///
/// `recent(count)` is implemented but expensive -- it re-reads the
/// tail of the file. Callers should prefer ``InMemoryAuditLog`` for
/// hot-path queries and use this implementation for durable persistence.

#include <cstdio>
#include <memory>
#include <mutex>
#include <string>

#include "application/ports/audit_log.hpp"

namespace pvpgn::infra::audit {

/// Factory result -- nullptr if the path cannot be opened for append.
class FileAuditLog : public application::ports::IAuditLog {
public:
    /// Open `path` in append mode. Throws via assertion on failure;
    /// prefer ``try_open`` for non-fatal handling.
    explicit FileAuditLog(const std::string& path);
    ~FileAuditLog() override;

    FileAuditLog(const FileAuditLog&)            = delete;
    FileAuditLog& operator=(const FileAuditLog&) = delete;

    /// Returns nullptr if the path cannot be opened.
    static std::unique_ptr<FileAuditLog> try_open(const std::string& path);

    void record(const application::ports::AuditEntry& entry) override;

    std::vector<application::ports::AuditEntry>
    recent(std::size_t count) const override;

    /// Encode a single entry as a TSV line (terminator included).
    /// Exposed for testing.
    static std::string format_line(const application::ports::AuditEntry& entry);

    /// Parse a TSV line written by `format_line`. Returns false on
    /// malformed input. Exposed for testing.
    static bool parse_line(const std::string& line,
                           application::ports::AuditEntry& out);

private:
    std::string         path_;
    std::FILE*          fp_ = nullptr;
    mutable std::mutex  mutex_;
};

}  // namespace pvpgn::infra::audit
