# R250 — AuditEntry source_ip + FileAuditLog NDJSON

## Checklist

- [x] Added `source_ip` field to `AuditEntry` struct
- [x] `FileAuditLog::format_line()` writes NDJSON format
- [x] `FileAuditLog::parse_line()` reads NDJSON format
- [x] `InMemoryAuditLog` updated if needed (no changes required — stores `AuditEntry` struct directly)
- [x] All `AuditEntry` construction call sites updated with `source_ip`
- [x] Existing tests updated for new format
- [x] No new third-party dependencies added
- [x] `file_audit_log.cpp` added to `infra/audit/CMakeLists.txt` (was missing)

## Exit Criterion

`FileAuditLog::format_line(entry)` returns a valid JSON object string.
`FileAuditLog::parse_line(line)` round-trips correctly.
All existing audit tests pass.

## Status: GREEN
