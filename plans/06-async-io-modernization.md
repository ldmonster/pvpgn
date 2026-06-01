# 06 — Async I/O Modernization

## What

Replace `src/common/fdwatch*` (epoll / kqueue / poll / select switch)
with a single, modern async runtime. End state: one I/O implementation
in `src/infra/net/`, no platform `#ifdef` outside that folder.

## Why

- `fdwatch` is a hand-rolled event loop with four backends and
  scattered lifecycle bugs. It blocks coroutines, structured
  concurrency, and timeouts-as-first-class-citizens.
- Every connection handler today fakes async via a state machine
  driven from the main loop. Coroutines collapse that into linear
  code.

## Decision (ADR required)

Choose **one** of:

- **A. Standalone Asio** (header-only, mature, used in
  `vcpkg`, no Boost dependency). Lowest friction; well-understood
  threading model.
- **B. `std::execution` (P2300)** when the toolchain matrix supports
  it. Most modern; smallest dependency footprint; least battle-tested.
- **C. liburing + epoll fallback** wrapped in a thin internal API.
  Highest performance ceiling, highest maintenance cost.

Recommendation: **A (Asio)** for wave two; revisit B once GCC 15 /
MSVC 19.4x ship `std::execution` in production form.

Record the decision in `docs/adr/0006-async-runtime.md`.

## Prerequisites

- Plan 02 in flight (so `fdwatch` is already isolated to one consumer).
- Plan 03 done (legacy `bnetd` does not directly call `fdwatch`).

## Concrete steps

1. **ADR** decision merged.
2. **vcpkg** entry for `asio` added; reproducible build green.
3. **Port abstraction.** Add `core/net/io_context.hpp` exposing the
   minimum surface our handlers need (post, spawn, deadline_timer,
   tcp acceptor/socket). No leaky `asio::` types in `core/`.
4. **Adapter.** Implement in `src/infra/net/asio/`.
5. **Migrate one listener at a time** (telnet → bnet → irc → wol →
   webui), each in its own PR. Each PR converts the handler to a
   coroutine (`task<void>`) and deletes the matching `fdwatch`
   subscription.
6. **Delete** `src/common/fdwatch*` and `src/common/network.{cpp,h}`
   once no consumer remains.
7. **Timeouts.** Every handler now has an explicit deadline; defaults
   live in `bnetd.toml` under `[net.timeouts]`.

## Acceptance criteria

- [ ] No file under `src/` includes `fdwatch.h`.
- [ ] `src/infra/net/` is the only directory with `#ifdef _WIN32` or
      `#ifdef __linux__` for I/O.
- [ ] Every TCP handler exposes a configurable timeout that an
      integration test exercises.
- [ ] Idle-connection memory footprint regression test passes within
      a 10% budget vs the pre-migration baseline.

## Risks

- Behavioural regressions in edge cases (half-close, RST, partial
  reads). Mitigate with a packet-replay fixture per protocol family
  added before each migration PR.
- Plugin scripts running long callbacks now stall a coroutine instead
  of the loop. Document and add an upper-bound timeout for Lua
  callbacks (plan 12).

## Out of scope

- Switching to UDP for any existing TCP protocol.
- Multithreading across CPU cores (still single-threaded I/O thread
  + worker pool for blocking work).
