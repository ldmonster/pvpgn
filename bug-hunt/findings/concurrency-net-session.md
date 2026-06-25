# Concurrency bug hunt — threaded networking + session layer

Subsystem: `src/infra/net/`, `src/app/bnetd/`, shared services injected into per-session FSMs.
Reference: original `pvpgn-server` was an event-loop, single-threaded select() server; v3 runs asio on a worker-thread **pool**, so all of the races below are NEW hazards the original never had.

## Is the runtime actually multi-threaded? YES.

- `IoRuntime::run(threads)` spawns **N worker threads**, each calling `ctx_.run()` on the *same* `io_context`.
  `src/infra/net/src/io_runtime.cpp:51-54`.
- `main.cpp:467-473` computes the thread count as
  `cfg.worker_threads > 0 ? cfg.worker_threads : std::max(1u, std::thread::hardware_concurrency())`.
  **Default = hardware_concurrency()**, i.e. genuinely multi-threaded on any normal box.
- The fiber path (`install_fiber_scheduler`) coerces to 1 thread, but bnetd's `rt.run(n_threads)` is called with `install_fiber_scheduler == false`, so the multi-thread pool is what runs in production.

### What IS correctly serialized (not bugs)
- **`infra::net::TcpSession`** binds *every* op to a per-session `strand_` (`make_strand`), and uses `shared_from_this()` in every async handler. Per-connection read/write/idle-timer/close are therefore serialized even on the pool, and the socket outlives in-flight ops. The extra `mu_` mutex is redundant-but-harmless. Good.
- **Per-connection FSM dispatch** (`BnetFsm::handle`, `framer->feed`, `ConnectionFsm::dispatch`, BNFTP/WOL `on_bytes`) all run *inside* the strand-bound `on_bytes` callback, so a single connection's FSM state is not raced by itself.
- **Cross-session sends** go through `TcpSession::send`, which `post`s to the *target* session's strand — safe from any thread.
- `g_next_session_id` is `std::atomic` (`main.cpp:174`). Good.
- `SessionManager` (`session_manager.cpp`) is correct: `shared_mutex`, copy-live-under-lock-then-invoke-outside in `for_each_session`, `weak_ptr` lifetime. (Note: `find_session`/`for_each_session` are currently **never called** outside tests — the cross-session broadcast hazard is latent, not active yet.)

---

## HIGH / CRIT findings

Count: **3 HIGH/CRIT data races** on shared mutable state reachable from multiple worker threads, plus 2 MEDIUM and 1 latent/correctness note.

### [CRIT] 1 — Global Lua VM driven concurrently from every session (no serialization)
- v3 ref: `g_lua_runtime` defined `main.cpp:169` (one global `LuaRuntime`); wrapped per-session in `LuaConnectionContext` at `src/app/bnetd/src/main/bnet_bnftp_dispatch.cpp:75`; invoked unguarded in `src/app/bnetd/src/lua_connection_context.cpp:65,75,84,100,114,123` via `runtime_.call_hook(...)`.
- The class itself documents it: `lua_runtime.hpp:22-25` — *"A single LuaRuntime instance is NOT thread-safe. The caller is responsible for serialising access (e.g. via a strand or mutex)."* No such serialization exists.
- Racing accesses: `lua_State*` is mutated by `lua_pcall`/stack ops. Every authenticated login, channel join/leave, game create/join/leave fires a hook. Two sessions on two worker threads → two concurrent `call_hook` on the **same `lua_State*`**.
- Interleaving: session A on thread 1 runs `on_channel_joined` → `call_hook("handle_channel_userjoin", ...)` (pushes args, `lua_pcall`); session B on thread 2 simultaneously runs `on_authenticated` → `call_hook("handle_user_login", ...)`. Both push onto the shared Lua stack and run the GC/interpreter concurrently → stack corruption / heap corruption / crash. Classic Lua-VM data race.
- Fix: serialize all Lua access. Either (a) a dedicated single strand/worker that owns the VM and `post()`s every hook to it (cleanest), or (b) a `std::mutex` inside `LuaRuntime` taken in `call_hook`/`eval`/`load_file`. A per-session strand is NOT enough — the VM is shared across sessions.

### [HIGH] 2 — `InMemorySessionRegistry` has no synchronization at all
- v3 ref: `src/infra/inmemory/include/infra/inmemory/session_registry.hpp` — **no mutex anywhere**. Two `std::unordered_map`s (`account_for_session_`, `session_for_account_`).
- Wiring: single instance `session_reg` (`main.cpp:340`) injected into `LoginUser` (`main.cpp:388-389`) and the logout path; reached on **every login and every logout**, which run on the per-session strand of *different* sessions → different worker threads concurrently.
- Racing accesses: `attach` (lines 21-36, two `operator[]` inserts) and `detach` (38-43, two `erase`) WRITE both maps; `session_for`/`account_for`/`list` READ them. Concurrent `attach` from two logins ⇒ unsynchronized `unordered_map::operator[]` insert (possible rehash) racing with another insert/read ⇒ UB (corrupted buckets, use-after-free of nodes, infinite loop on lookup).
- Bonus check-then-act: even single-call-locked, `attach` reads `contains()` then inserts (lines 23-34) — two logins of the same account could both pass the "no existing session" check. The "one account = one session" invariant is not atomic.
- Interleaving: client A and client B (different accounts) both finish NLS/OLS login at the same instant on threads 1 and 2; both call `attach` → concurrent rehash of `account_for_session_` → corruption.
- Fix: add a `std::mutex`/`std::shared_mutex` like the sibling repos (`account_repository.hpp`, `channel_repository.hpp` already have one) and take it for the whole `attach`/`detach` body so the check-then-act is atomic too.

### [HIGH] 3 — `InMemoryEventBus` map raced; only `next_id_` is atomic
- v3 ref: `src/infra/inmemory/include/infra/inmemory/event_bus.hpp` — comment says *"Synchronous, single-threaded… Safe for unit tests and single-fiber composition."* No mutex on `handlers_`.
- Wiring: single `event_bus` (`main.cpp:342`) injected into `LoginUser` and `CreateAccount` (`main.cpp:388-391`) and `BnetdService` (`main.cpp:371`). `publish(...)` is fired on every login/account-creation event → from multiple worker threads.
- Racing accesses: `publish` (line 20) does `const auto snapshot = handlers_;` — a full **copy of the unordered_map without a lock** — while `subscribe` (line 34, `handlers_.emplace`) / `unsubscribe` (line 40, `handlers_.erase`) mutate it. `next_id_` being atomic does nothing for the map itself.
- Interleaving: subscriptions are mostly wired at startup, so the hottest race is two threads in `publish` copying `handlers_` while a late `subscribe`/`unsubscribe` mutates it; also two `publish` copies concurrent with any structural change. Concurrent read-copy + write of `unordered_map` = UB.
- Fix: guard `handlers_` with a `std::mutex` (copy the snapshot under the lock, then invoke handlers outside it, mirroring `SessionManager::for_each_session`).

---

## MEDIUM findings

### [MED] 4 — `TcpAcceptor::open_` is a plain `bool` raced between worker and owner thread; accept handler captures raw `this`
- v3 ref: `src/infra/net/src/tcp_acceptor.cpp`. `open_` written by `close()` on the owner/main thread (`:89-90`) and **read inside the async_accept completion handler on a worker thread** (`:112 if (open_) do_accept();`). Declared `bool open_ = false;` (`tcp_acceptor.hpp:92`) — non-atomic ⇒ data race / torn read, and the "stop accepting" signal may be missed or re-trigger.
- The accept handler also captures **raw `this`** (`tcp_acceptor.cpp:101`, `[this]`), with no `shared_from_this`. The acceptor is destroyed at server shutdown (`TcpListener::stop()`/dtor, `main.cpp:481-484`). If an `async_accept` completes on a worker after the acceptor is destroyed, `this`, `open_`, `factory_`, `raw_handler_` are dangling ⇒ UAF. The accept loop is self-chained (one accept in flight at a time), so this is a shutdown-window UAF, not a steady-state one — hence MEDIUM.
- Fix: make `open_` `std::atomic<bool>` (or run the acceptor on its own strand and post `close()` to it), and either give `TcpAcceptor` a strand + cancel-and-drain on close, or capture a `weak_ptr`/guard so the completion handler bails if the acceptor is gone.

### [MED] 5 — Use-case-level check-then-act on the (individually-locked) repos
- The in-memory repos (`account_repository.hpp`, `channel_repository.hpp`, `game_repository.hpp`) are each internally `shared_mutex`-locked **per call**, but the use-cases do `find_by_name(...)` then `save(...)` as two separate locked calls. With the multi-thread pool, two concurrent `CreateAccount`/channel-create/`save` for the same name can both pass the "not found" check and both `save`, clobbering one another. (Cf. the recent commit "Fix silent account clobber in SqlAccountRepository::save" — same shape exists in the inmemory + use-case layer.)
- Also `InMemoryGameRepository::find_by_*` hands out `shared_ptr<Game>` to a **shared mutable domain object** (`game_repository.hpp:23-43`); two sessions mutating the same `Game` concurrently is unsynchronized. `IAccountRepository`/`IChannelRepository` return by **value** (copies) and are safe in this respect.
- Severity MEDIUM because it depends on two clients hitting the same key concurrently; data loss/duplicate rather than memory corruption.
- Fix: push the read-modify-write into a single locked repository method (compare-and-set / `insert_if_absent`), or serialize account/channel/game creation through one strand.

---

## Latent / correctness note (not a race)

### [NOTE] `AsioEventLoop` injected into BnetdService is never run
- `event_loop` is constructed (`main.cpp:265`) and injected into `BnetdService` (`main.cpp:366`), but **`event_loop.run()` is never called** anywhere in `main.cpp` (grep: only the include, the ctor, and the injection). Its `io_context` is never driven, so anything `post()`ed onto it via `BnetdService` silently never executes. This is a separate `io_context` from the `IoRuntime` (`rt`) that actually carries network traffic. Worth flagging to whoever relies on event-loop scheduling inside BnetdService — it's a liveness bug, not a concurrency one.

---

## Summary table

| # | Sev  | File | Bug |
|---|------|------|-----|
| 1 | CRIT | lua_connection_context.cpp / main.cpp:169 | Global Lua VM (`lua_State*`) called concurrently from all sessions, no lock/strand |
| 2 | HIGH | inmemory/session_registry.hpp | Two unordered_maps mutated/read from multiple threads with NO mutex + check-then-act |
| 3 | HIGH | inmemory/event_bus.hpp | `handlers_` map copied/mutated across threads, only `next_id_` is atomic |
| 4 | MED  | net/tcp_acceptor.cpp/.hpp | non-atomic `open_` raced; accept handler captures raw `this` → shutdown-window UAF |
| 5 | MED  | use-cases + inmemory repos | find-then-save check-then-act; `Game` shared_ptr handed out mutable |
| – | NOTE | main.cpp + asio_event_loop.cpp | BnetdService's `AsioEventLoop` is never `run()` (liveness) |
