# 09 · d2cs & d2dbs Refactor

`d2cs` (Diablo II Closed Realm Server) and `d2dbs` (Diablo II Database
Server) duplicate 80 % of `bnetd`'s boilerplate. The refactor extracts
that boilerplate into `src/runtime/` and folds each service into the
same hexagonal architecture.

## 1. Shared service host

All three binaries share:

```
src/runtime/
├── service_host.cpp       # owns IoRuntime, signals, lifecycle
├── cli.cpp                # CLI11-based; --config, --foreground, …
├── daemonize.cpp          # POSIX fork / setsid / umask
├── win_service.cpp        # Win32 service control manager glue
├── composition_root.hpp   # template hook called by each service
└── crash_handler.cpp      # backtrace via boost::stacktrace
```

So `services/d2cs/main.cpp` becomes:

```cpp
int main(int argc, char** argv) {
    return run_service<D2csComposition>(argc, argv);
}
```

This eliminates the duplicated `main.cpp`, `prefs.cpp`, `cmdline.cpp`,
`handle_signal.cpp`, `server.cpp` between bnetd/d2cs/d2dbs.

## 2. d2cs

### 2.1 Responsibilities
* Authenticates with bnetd over the d2cs↔bnetd protocol.
* Accepts D2 clients on its own TCP port and routes them to a D2GS
  (game server).
* Maintains a character-locking and queueing system.

### 2.2 Module mapping

| Legacy file (`src/d2cs/`)        | New location |
|---|---|
| `main.cpp`,`cmdline.cpp`,`prefs.cpp`,`handle_signal.cpp` | `runtime/*` (shared) |
| `server.cpp`,`net.cpp`           | `infrastructure/net/io_runtime`, `tcp_acceptor`, `udp_endpoint` |
| `connection.cpp/.h`              | `infrastructure/net/tcp_session` + `protocol/d2cs/fsm` |
| `handle_bnetd.cpp`,`handle_d2cs.cpp`,`handle_d2gs.cpp`,`handle_init.cpp` | `protocol/d2cs/fsm.cpp`, `protocol/d2gs/fsm.cpp` |
| `s2s.cpp`,`serverqueue.cpp`,`gamequeue.cpp` | `infrastructure/net/peer_link.cpp`, `application/realm/gs_queue.cpp` |
| `bnetd.cpp` (s2s client to bnetd) | `infrastructure/net/peer_link.cpp` (TLS-capable) |
| `d2charfile.cpp`,`d2charlist.cpp`,`d2gs.cpp`,`d2ladder.cpp`,`game.cpp` | `domain/realm/{character,realm,gs,ladder}.cpp` + `infrastructure/persistence/realm/*` |

### 2.3 Notable improvements

* The d2cs↔bnetd link becomes a typed peer connection
  (`PeerLink<DcsBnetdProtocol>`) with health-check, exponential
  back-off, and TLS option.
* Character locking moves from in-memory list to a Repository that can
  be backed by Redis or DB when running multiple d2cs instances behind
  the same realm (a future option, not delivered in v3.0).
* Per-realm config: `realm.toml` lists allowed GS endpoints with
  capabilities (Open/Closed, normal/hardcore, ladder/non-ladder).

## 3. d2dbs

### 3.1 Responsibilities
* Owns D2 character `.d2s` files and ladder snapshots.
* Talks to d2cs (and indirectly to d2gs) to load/save characters.

### 3.2 Module mapping

| Legacy file (`src/d2dbs/`)       | New location |
|---|---|
| `main.cpp`,`cmdline.cpp`,`prefs.cpp`,`handle_signal.cpp` | `runtime/*` |
| `dbserver.cpp`,`dbspacket.cpp`   | `infrastructure/net/io_runtime` + `protocol/d2gs/fsm` |
| `charlock.cpp`                   | `domain/realm/character_lock.cpp` |
| `dbsdupecheck.cpp`               | `domain/realm/dupe_checker.cpp` |
| `d2ladder.cpp`                   | `application/realm/ladder_snapshot.cpp` |

### 3.3 Improvements

* `.d2s` parsing extracted into `protocol/d2save/codec.cpp` — a pure
  function over bytes, fuzzed in CI.
* Dupe-check rules (item GUID registry) become injectable strategies.
* On-disk layout migrated to a content-addressable structure
  (`chars/<2hex>/<full-hash>/`) preventing the legacy O(n) directory
  scan; old layout still readable through a migration tool.

## 4. Inter-service security

Today the d2cs↔bnetd and d2cs↔d2dbs links are plaintext TCP with a
shared secret. The new design:

* Mutual TLS (rustls/openssl 3) with pinned certificates.
* Capability tokens (signed JWT) for cross-service calls; a compromised
  d2dbs cannot impersonate d2cs.
* Configurable Unix-socket transport for co-located deployments.

## 5. Optional: single-binary mode

Because all three services share `runtime/`, a build flag
`PVPGN_SINGLE_BINARY=ON` produces a single `pvpgn` executable that
hosts bnetd/d2cs/d2dbs in one process (separate fiber pools). Useful
for hobby/dev deployments. Production keeps them as separate
processes.

## 6. Removal of dead targets

* `bnproxy/` — no longer maintained, no users. Archived to a `legacy/`
  branch, removed from main build.
* `bnpcap/` — relies on libpcap with bit-rot patches. Archived.
* `bntrackd/` — UDP master tracker; kept but rewritten on top of
  `runtime/` (~200 lines).
* `client/` (`bnchat`, `bnstat`, `bnftp`, `bnbot`) — `bnstat` and
  `bnchat` are useful diagnostics; kept under `tools/`. `bnbot` is
  deprecated in favor of the web UI's interactive console.
* `bniutils/` — image format CLIs, kept verbatim under `tools/`.
