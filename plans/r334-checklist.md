# R334 Checklist — `core::trace::Span` + tracing ports

## Goal
Introduce a lightweight, zero-dependency distributed-tracing primitive into
the `core` library.  Provide a port interface (`ITraceSink`) so that the
concrete exporter (log, OTLP, no-op) can be swapped without touching domain
or application code.

---

## Deliverables

- [x] **`src/v3/core/include/core/trace.hpp`**
  - `SpanContext` struct (`trace_id`, `span_id`, `parent_span_id`)
  - `Span` RAII class (pImpl, move-only)
    - `set_attribute(key, value)`
    - `set_status_ok()` / `set_status_error(message)`
    - `context()`, `name()`, `start_time()`, `end_time()`, `is_error()`, `error_message()`
  - `SpanSink` typedef (`std::function<void(const Span&)>`)
  - `set_global_span_sink(SpanSink)` / `get_global_span_sink()`
  - `PVPGN_SPAN(name)` convenience macro

- [x] **`src/v3/core/src/trace.cpp`**
  - `random_hex(byte_count)` using `std::mt19937_64` + mutex
  - `Span::Impl` with name, context, start/end times, attributes, error state
  - Constructor generates 32-char `trace_id` (16 bytes) and 16-char `span_id` (8 bytes)
  - Destructor records end time and calls global sink
  - Global sink stored in `g_sink` protected by `g_sink_mu`

- [x] **`src/v3/application/ports/include/application/ports/trace_sink.hpp`**
  - `ITraceSink` port with `virtual void record(const Span&) noexcept = 0`

- [x] **`src/v3/CMakeLists.txt`** — `core/src/trace.cpp` added to `core` library SOURCES

---

## Constraints satisfied

- [x] SPDX header on all new files
- [x] `#pragma once` on all headers
- [x] No new third-party dependencies in `core`
- [x] Thread-safe global sink (mutex-protected)
- [x] Move-only `Span` (copy deleted)
- [x] pImpl idiom keeps `Span::Impl` out of the public header
