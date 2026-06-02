# ADR 0006: Async I/O Runtime — Boost.Asio + Boost.Fiber

**Date**: 2026-06-02
**Status**: Accepted
**Deciders**: PvPGN Core Team

## Context

The legacy networking core (`src/common/fdwatch*` + `src/common/network.*`)
was a hand-rolled event loop with four interchangeable backends
(epoll / kqueue / poll / select) selected by `#ifdef`. Problems:

- Four backends multiplied platform `#ifdef`s and lifecycle bugs across the
  codebase, and the I/O concern leaked far outside any single module.
- Every connection handler faked asynchrony as a state machine driven from
  the global main loop, so handler logic was inverted, hard to read, and hard
  to unit-test.
- Timeouts, half-close / RST handling, and back-pressure were all ad-hoc.

Plan 06 (`plans/06-async-io-modernization.md`) required replacing this with a
single modern async runtime confined to `src/infra/net/`, with no I/O-related
platform `#ifdef` outside that directory, and asked this ADR to record the
runtime choice. The plan enumerated three candidates:

- **A. Standalone Asio** — header-only, mature, no Boost dependency.
- **B. `std::execution` (P2300)** — most modern, smallest dependency, least
  battle-tested; not yet shipping in the supported toolchain matrix.
- **C. liburing + epoll fallback** — highest performance ceiling, highest
  maintenance cost, Linux-centric.

## Decision

Adopt **Boost.Asio** as the I/O reactor/proactor, paired with **Boost.Fiber**
for per-connection handlers. This is option **A** (Asio) realised through the
Boost distribution rather than standalone Asio, plus Boost.Fiber on top.

The runtime lives entirely under `src/infra/net/`:

- `io_runtime` — owns one `boost::asio::io_context` and an N-thread worker
  pool; exposes an executor handle. Time/back-pressure primitives:
  `steady_timer`, `strand`, `executor_work_guard`.
- `tcp_acceptor` / `tcp_session` / `udp_endpoint` / `socket` — thin Asio
  wrappers; `signal_handler` uses `asio::signal_set`.
- `fiber_pool` / `fiber_session` — each accepted connection runs in a
  Boost.Fiber, so handler code is written in a linear, synchronous
  "read-loop" style (`while (auto bytes = chan.recv()) { ... chan.send(...) }`)
  while remaining non-blocking on the underlying `io_context`.

Boost.Fiber was chosen over C++20 coroutines (`co_await`) because it delivers
the same linear-handler ergonomics today, across the whole supported toolchain
matrix, without requiring every protocol handler to be rewritten as a coroutine
type or pulling in a third-party coroutine-over-Asio shim.

`std::execution` (B) is deferred until it ships in production GCC/MSVC;
liburing (C) was rejected as premature optimisation with a Linux-only,
high-maintenance profile.

Dependencies are pinned via vcpkg (`boost-asio`, `boost-fiber`,
`boost-context`, `boost-system`, …) and provided by distro packages in the
Docker images (`boost-dev`).

## Consequences

Positive:

- `src/common/fdwatch*` and the four-backend `#ifdef` switch are deleted; no
  file under `src/` includes `fdwatch.h`.
- All v3 listeners (bnet, irc, wol, bnftp/file, d2cs) run on one runtime with
  fiber-based sessions; handler code is linear and unit-testable against a
  fake session channel.
- I/O platform `#ifdef`s are confined to `src/infra/net/`. (Address-string and
  time helpers under `core/net`, `core/time` still include platform socket /
  time headers for `inet_ntop`/`gmtime`-style formatting; these are value
  utilities, not the I/O reactor, and are tracked separately.)
- First-class timeouts via `steady_timer`; graceful shutdown via
  `ShutdownCoordinator` + `asio::signal_set`.

Negative / costs:

- Adds a Boost dependency (asio + fiber + context). Heavier than standalone
  Asio, but Boost is already required and widely packaged.
- A long-running Lua/plugin callback now stalls its fiber rather than the whole
  loop; per-callback upper-bound timeouts are owned by Plan 12.
- Fiber stacks cost memory per idle connection; the idle-footprint budget is
  tracked by Plan 06's regression-test acceptance criterion.

## Status of Plan 06 acceptance criteria

- [x] No file under `src/` includes `fdwatch.h`.
- [~] `src/infra/net/` is the only directory with I/O `#ifdef`; remaining
  platform `#ifdef`s under `core/net` and `core/time` are address/time value
  helpers, not the I/O reactor.
- [ ] Every TCP handler exposes a configurable timeout exercised by an
  integration test (`[net.timeouts]` in `bnetd.toml`).
- [ ] Idle-connection memory-footprint regression test within a 10% budget.
