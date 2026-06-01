// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file file_audit_log.hpp
/// Append-only file implementation of IAuditLog.
///
/// Each `record()` flushes a single NDJSON line (one JSON object per line):
///   {"ts":"2024-01-01T00:00:00.000Z","action":"AccountCreated","actor":42,
///    "subject":"user","details":"some detail","source_ip":"192.168.1.1"}
/// String fields are JSON-escaped. `source_ip` is omitted when empty.
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

#include "domain/moderation/ports.hpp"

namespace pvpgn::infra::audit {

/// Factory result -- nullptr if the path cannot be opened for append.
class FileAuditLog : public domain::moderation::IAuditLog {
public:
    /// Open `path` in append mode. Throws via assertion on failure;
    /// prefer ``try_open`` for non-fatal handling.
    explicit FileAuditLog(const std::string& path);
    ~FileAuditLog() override;

    FileAuditLog(const FileAuditLog&)            = delete;
    FileAuditLog& operator=(const FileAuditLog&) = delete;

    /// Returns nullptr if the path cannot be opened.
    static std::unique_ptr<FileAuditLog> try_open(const std::string& path);

    void record(const domain::moderation::AuditEntry& entry) override;

    std::vector<domain::moderation::AuditEntry>
    recent(std::size_t count) const override;

    /// Encode a single entry as an NDJSON line (newline terminator included).
    /// Exposed for testing.
    static std::string format_line(const domain::moderation::AuditEntry& entry);

    /// Parse an NDJSON line written by `format_line`. Returns false on
    /// malformed input. Exposed for testing.
    static bool parse_line(const std::string& line,
                           domain::moderation::AuditEntry& out);

private:
    std::string         path_;
    std::FILE*          fp_ = nullptr;
    mutable std::mutex  mutex_;
};

}  // namespace pvpgn::infra::audit
