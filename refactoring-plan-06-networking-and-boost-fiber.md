# 06 · Networking & Boost.Fiber Integration

This is the most architecturally significant change. The legacy server
is a **single-threaded reactor** built on hand-rolled `fdwatch` that
multiplexes raw `t_packet` parsing and business logic in the same call
stack. We replace it with **Boost.Asio** for I/O multiplexing and
**Boost.Fiber** for cooperative scheduling of per-session logic.

## 1. Why fibers and not coroutines / threads?

| Option | Pros | Cons |
|---|---|---|
| Thread per connection | Easiest mental model | Tens of thousands of threads, lock contention, large stack overhead, mismatch with current single-thread invariants. |
| Native `boost::asio` callbacks/`co_await` (C++20 coroutines) | Zero-overhead, modern | Coroutine refactor is intrusive across all handlers; non-coroutine code in the codebase becomes second-class. Lua/sol3 cannot easily `co_await`. |
| **Boost.Fiber on top of Asio (`yield`-style)** | Code looks synchronous; thousands of fibers per OS thread; fibers can call into legacy synchronous code (e.g. SQLite, Lua) without rewriting them; integrates with Asio via `boost_fibers_asio`. | Stack-per-fiber cost (default 64 KiB; configurable to 16 KiB or 32 KiB pooled). Bridge code needed for Asio. |

We pick **fibers** because:

* It allows incremental migration: a legacy handler becomes a fiber
  with zero changes other than running on a fiber stack.
* Lua scripts can transparently `socket.read()` because the C++ side
  yields the fiber, not the OS thread.
* SQLite/MySQL synchronous APIs become fiber-yielding via a small
  thread-pool bridge — no rewrite of repository code.
* Determinism: tests can drive a single-threaded fiber scheduler and
  reproduce ordering bugs.

## 2. Runtime topology

```
┌─────────────────────────────────────────────────────────────────┐
│ OS process (bnetd)                                              │
│                                                                 │
│ ┌────────────────────────────────────────────────────────────┐  │
│ │ Asio io_context (one per worker thread, N = #cores)        │  │
│ │ ┌───────────────────┐  ┌───────────────────┐               │  │
│ │ │ Fiber scheduler   │  │ Fiber scheduler   │   ...         │  │
│ │ │   - acceptors     │  │   - sessions      │               │  │
│ │ │   - sessions      │  │                   │               │  │
│ │ └───────────────────┘  └───────────────────┘               │  │
│ └────────────────────────────────────────────────────────────┘  │
│                                                                 │
│ ┌──────────────────────────┐  ┌─────────────────────────────┐   │
│ │ DB-blocking thread pool  │  │ CPU/heavy thread pool       │   │
│ │ (libsqlite3, ODBC)       │  │ (hash, crypto, image …)     │   │
│ └──────────────────────────┘  └─────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

Two work-stealing schedulers running on N worker threads share fibers.
Fibers pinned to a single io_context for protocols that demand strict
ordering (e.g. the BNet binary FSM per session); cross-context fibers
for stateless work (HTTP, metrics).

## 3. The I/O runtime module (`infrastructure/net/`)

```cpp
class IoRuntime {
public:
    explicit IoRuntime(IoConfig cfg);                 // threads, pool size
    void run();                                       // blocks until stopped
    void stop();
    boost::asio::io_context& io();                    // for adapters
    boost::fibers::buffered_channel<>& shutdownChan();
    template <class Fn> void spawn(Fn&&);             // launch fiber
};
```

* Internally configures `boost::fibers::use_scheduling_algorithm<
  boost::fibers::algo::work_stealing>` with the Asio integration
  (`boost::fibers::asio::yield`).
* Owns signal-handling (`SIGINT`, `SIGTERM`, `SIGHUP`, `SIGUSR1/2`) via
  `boost::asio::signal_set`. Replaces the legacy `static volatile`
  flags.

## 4. TCP listener and session

```cpp
class TcpAcceptor {
public:
    TcpAcceptor(IoRuntime&, Endpoint, std::shared_ptr<ISessionFactory>);
    void start();
    void stop();
};

class TcpSession : public std::enable_shared_from_this<TcpSession> {
public:
    void run();                       // entry fiber
    void send(std::span<const std::byte>);    // queues bytes
    void close();
private:
    boost::asio::ip::tcp::socket socket_;
    boost::fibers::buffered_channel<OutgoingFrame> outq_;
    std::shared_ptr<IProtocolHandler> protocol_;
};
```

`TcpSession::run()` is the per-session fiber:

```cpp
void TcpSession::run() {
    auto self = shared_from_this();
    boost::system::error_code ec;
    // reader fiber:
    while (!stopRequested_) {
        auto frame = protocol_->readNext(socket_, yield[ec]);
        if (ec) break;
        protocol_->handle(std::move(frame), context_);
    }
    // writer fiber spawned separately
}
```

* All handlers run synchronously inside the fiber.
* Blocking on DB / file I/O yields the fiber, not the thread.
* Backpressure: bounded buffered channels for both directions.

## 5. Protocol handler abstraction

```cpp
class IProtocolHandler {
public:
    virtual ~IProtocolHandler() = default;

    // Reads one framed PDU. May yield.
    virtual tl::expected<Frame, NetError>
        readNext(boost::asio::ip::tcp::socket&, FiberYield) = 0;

    // Pure, no I/O. Drives the FSM, may call back via `out`.
    virtual void handle(Frame, SessionContext& ctx) = 0;
};
```

Implementations live in `protocol/<name>/` and emit application-layer
commands via `SessionContext` (a thin facade exposing `IMessageRouter`
+ `ICommandDispatcher`). See section 07.

## 6. The reactor → fiber translation

For each legacy `handle_*_packet(t_connection*, t_packet*)`:

1. Wrap the packet/connection in the new `Frame` / `SessionContext`.
2. Compile the legacy handler unchanged into a fiber-friendly TU.
3. Schedule it via `IoRuntime::spawn(...)` — the legacy reactor goes
   away.

This is a **mechanical** transformation, executed file-by-file, and
buys us continuous behavior parity.

## 7. UDP, files, signals

* UDP keepalive: `boost::asio::ip::udp::socket` + a fiber pumping
  datagrams.  No more dedicated handler thread.
* File transfer (`bnftp`): per-file fiber that `co_yields` after each
  chunk, allowing rate-limit fairness.
* Signals: `asio::signal_set`; `SIGHUP` reloads config, `SIGUSR1`
  triggers `accountlist_save()` (via `SaveAll` use-case),
  `SIGTERM/INT` triggers graceful shutdown saga.

## 8. Cross-fiber synchronisation primitives

* `boost::fibers::mutex`, `boost::fibers::condition_variable` for
  protected aggregates (when one fiber writes while many read).
* `boost::fibers::buffered_channel<T>` for producer/consumer (e.g.
  outgoing send queue, event-bus dispatch).
* `boost::fibers::future<T>` for one-shot async results (e.g. DB
  query handed to thread pool).
* For most aggregates we will still use **single-fiber affinity**:
  bind each `Account`/`Channel`/`Game` aggregate to a specific
  io_context so mutation never races. The application layer enforces
  this with `strand`-like wrappers around repositories.

## 9. Fiber stack sizing

* Default 32 KiB; pooled, segmented when supported.
* Long-running sagas may opt into 128 KiB.
* `BOOST_USE_VALGRIND` enabled in debug builds.
* For 10 000 concurrent sessions × 32 KiB = ~320 MiB virtual.

## 10. Backpressure and overload

* Each `TcpSession` has a bounded send channel (default 64 frames).
  When full, the producer fiber blocks instead of allocating
  unbounded; this prevents the memory-explosion behavior seen today
  when a slow client wedges the writer.
* `IoRuntime` exposes per-protocol counters (`packets_in`,
  `bytes_out`, `pending_sends`) to Prometheus.

## 11. Graceful shutdown

* Composition root posts `ShutdownRequested` event.
* Acceptors stop, existing sessions are told `closeWhenIdle()`.
* A drain timer (configurable) waits for fibers to finish; after
  timeout, sockets are force-closed and fibers cancelled
  (cooperative — they check a `stop_token` on each `yield`).
* Final `SaveAll` use-case persists state. All in one fiber-saga.

## 12. Why this is better than today

| Concern | Today | After |
|---|---|---|
| Adding a new protocol | Edit `server.cpp`, `connection.cpp`, several `handle_*` files | Implement `IProtocolHandler`, register it in composition root. |
| Blocking on DB | Blocks **all** users | Yields only that fiber. |
| Listening on additional ports / TLS | Edit reactor, add fd flags | Add another `TcpAcceptor` with TLS context. |
| Unit-testing the network code | Effectively impossible | Inject a fake `IoRuntime` with virtual time. |
| Cross-platform timers/signals | Manual `setitimer` / SIGPIPE flags | Asio handles uniformly. |

## 13. Boost dependency footprint

We add: `boost::system`, `boost::asio`, `boost::fiber`,
`boost::context`, `boost::beast` (used in WebUI),
`boost::endian`, `boost::intrusive`. All header-only or small
statically-linked libs. Pinned via `FetchContent` or vcpkg manifest;
no system-wide Boost required.
