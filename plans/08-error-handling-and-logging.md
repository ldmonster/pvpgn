# 08 — Error Handling & Logging

**Goal:** One error model, one logging facade, one structured-log
schema. No exceptions for expected failure. No `printf`. No log
strings interpolated by hand.

## 1. Error model

- `core::Result<T, E>` (already exists). `E` defaults to
  `core::StatusCode`.
- `core::StatusCode` is an `enum class` covering every expected
  failure mode in the system (see `core/result.h` /
  `core/status_code.h` — extend, don't replace):

```
Ok, Cancelled, Unknown, InvalidArgument, NotFound, AlreadyExists,
PermissionDenied, ResourceExhausted, FailedPrecondition,
Aborted, OutOfRange, Unimplemented, Internal, Unavailable,
DataLoss, Unauthenticated, Conflict, RateLimited,
Timeout, ProtocolError, NetworkError, ConfigError,
DependencyFailed, SchemaMismatch
```

Each StatusCode has a `std::string_view to_string(StatusCode)` and a
`StatusCode from_errno(int)` translator (for infra adapter use).

**Exceptions are reserved for**:

- Unrecoverable startup failures in `services::*::build()`.
- Programmer-error contract violations (`assert`-equivalent —
  preferred mechanism is `PVPGN_VERIFY` macro, see below, which
  std::terminates).
- Third-party libraries we can't avoid (lua, boost::asio). Adapters
  catch at the boundary and translate to `Result`.

## 2. Asserts

`PVPGN_VERIFY(cond, "msg", arg1, ...)` macro (lives in
`core/contract.h`):

- In debug: logs FATAL with `source_location`, then `std::abort`.
- In release: logs FATAL once via the structured logger, then
  `std::terminate`. Never a no-op (an unrecoverable invariant
  violation is unrecoverable in all builds).

Use `PVPGN_VERIFY` for **invariants the caller controls**. Do not
use it as input validation — that's `StatusCode::InvalidArgument`.

## 3. Logging

Logging API (existing — `src/v3/core/include/pvpgn/core/logging.h`):

```cpp
namespace pvpgn::core::log {

enum class Level { Trace, Debug, Info, Warn, Error, Fatal };

void log(Level, std::string_view category, std::string_view msg);
void log(Level, std::string_view category, std::string_view msg,
         std::initializer_list<KV> kvs);

template<class... Args>
void info(std::string_view category, std::format_string<Args...> fmt,
          Args&&... args);  // similarly debug/warn/error/fatal
}
```

Discipline:

- **Category** is a stable string identifier (`"net.bnet"`,
  `"persist.sqlite"`, `"app.identity.login"`). Used for filtering.
- **No string interpolation in the call site.** Pass values as KV
  pairs (`{"username", uname}`) or as `std::format` args; the sink
  formats.
- **Levels**:
  - `trace` — every packet, only when explicitly enabled.
  - `debug` — sub-step decisions, useful when investigating.
  - `info` — lifecycle events ("listening", "shutdown",
    "account created").
  - `warn` — recoverable degradation.
  - `error` — operation failed but the daemon stays up.
  - `fatal` — daemon about to exit.

- **No PII in logs by default.** Usernames OK (already public on
  Battle.net); passwords / password hashes / IPs are not logged
  above `debug`. IP redaction at `info+` (mask last octet).

## 4. Sinks

`infra/log/` provides:

- `StderrSink` (text, colourised TTY-aware).
- `FileSink` (rotating, gz-compressed olds).
- `JsonSink` (newline-delimited JSON, for log shippers).
- `SyslogSink` (Linux), `EventLogSink` (Windows).

Composition root picks sinks from TOML config. Multiple sinks
allowed.

## 5. Audit log (separate)

Security-relevant events (login, password change, ban, op grant,
admin command) go to a **separate** audit log via `AuditSink`
port. Audit entries:

- Always JSON.
- Always include actor account id, action, target, timestamp,
  source IP.
- Append-only file by default. Optional adapter to ship to syslog
  / external store.

Audit and ordinary log are decoupled by design — the audit trail
must survive log-level filtering.

## 6. Tracing hooks

Optional, but the surface is reserved: `core::trace::Span` RAII
type with `MakeChild(...)`. Sinks adapter in `infra/tracing/`
(OpenTelemetry OTLP HTTP exporter). Enabled per-binary via TOML.
See `10-observability.md`.

## 7. Concrete tasks

- [ ] R270: finalise `StatusCode` enum (audit all `Result<T,
      StatusCode>` consumers, fill gaps).
- [ ] R271: add `PVPGN_VERIFY` macro; replace ad-hoc `assert()` in
      v3 code.
- [ ] R272: build `JsonSink` and audit pipeline.
- [ ] R273: structured-log audit — `grep` for `"%s"` / `<<` style
      strings in v3 code, replace with KV calls.
- [ ] R274: IP redaction policy in the standard formatter.
