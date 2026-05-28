# R251 — Retire infra/logging/eventlog_bridge

## Checklist

- [x] Read all files in infra/logging/
- [x] Found all consumers of eventlog_bridge.hpp
- [x] Migrated all consumers to core/format.hpp
- [x] Replaced eventlog_bridge.hpp with deprecation shim
- [x] eventlog_bridge.cpp emptied (implementation removed)
- [x] infra_logging CMakeLists.txt updated (removed spdlog dep)
- [x] No LOG_* macro name conflicts remain
- [x] No infra::logging::LogLevel usage remains in non-deprecated code

## Exit Criterion

`grep -r 'infra/logging/eventlog_bridge' src/v3/` returns only the shim file itself.
No `LOG_FATAL` macro usage remains in `src/v3/` (replaced by `LOG_CRITICAL`).

## Status: GREEN
