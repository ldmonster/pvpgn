# 05 — Ports & Adapters Discipline

**Goal:** Lock in the hexagonal boundary so that every external
dependency (DB, FS, clock, RNG, socket, lua VM, OS signals, time,
network, metrics) is reachable from `application/` only through a
**port** defined in `application/<bc>/ports/`. Adapters live in
`infra/<technology>/` and are wired in `services/`.

## 1. Port definition checklist

A port header (in `application/<bc>/ports/<port>.h`):

- Pure interface — only abstract methods + `virtual ~Port() = default;`
- All methods return `core::Result<T, StatusCode>` or `void`. No
  exceptions.
- All parameters are domain types or `core::Bytes` /
  `std::string_view` / `std::span<const std::byte>`. **Never** infra
  types (no `sqlite3*`, no `boost::asio::ip::tcp::socket`, no
  `lua_State*`).
- No default-implemented methods. NVI is fine but the public surface
  stays abstract.
- One file per port. Filename = interface name (without `I` prefix
  unless the interface name would otherwise collide).
- A `<port>_fake.h` in `infra/inmemory/<bc>/` provides the test fake.

## 2. Canonical ports (catalogue)

### Persistence
- `AccountRepository`
- `ChannelStore` (persistent channel definitions, not runtime state)
- `GameStore`
- `ClanRepository`
- `FriendListStore`
- `LadderRepository`
- `RealmDirectory`
- `MailStore`
- `NewsStore`
- `IpBanRepository`
- `TournamentRepository`

All persistence ports expose a `transactional<F>(F&&) -> Result<...>`
method or live behind a `UnitOfWork` port that owns the transaction
scope (decide once in R233).

### Runtime registries (in-memory, but still ports)
- `SessionStore` — logged-in connections.
- `ChannelRegistry` — live channels and membership.
- `GameDirectory` — live games.
- `RealmRegistry` — connected realm servers (d2cs heartbeats).

### Cross-cutting
- `Clock` (already in `core` — `core::Clock` is fine to consume directly
  since it's an abstract base, not infra).
- `RandomSource`.
- `EventBus` (in `core`).
- `MetricSink`, `TraceSink`, `AuditSink`.
- `Logger` (`core::log::Logger`).

### I/O
- `EventLoop` — start/stop, post task. Adapter: `infra/net/asio`.
- `Listener<Proto>` — accept connections for a given protocol.
- `Connection` — read/write/close abstraction. Adapter wraps asio
  socket.
- `Resolver` — DNS lookups. Adapter: asio.

### Scripting
- `ScriptHost` — load/eval/call. Adapter: `infra/lua`.
- `ScriptSandbox` — resource limits, memory caps, instruction count.

### Protocol
- `PacketCodec<P>` — encode/decode for a protocol (bnet, irc, telnet,
  wol). Implementations live in `protocol/<proto>/` and are
  technology-light enough to be in the protocol layer directly, not
  in infra.

## 3. Adapter rules

Adapter in `infra/<tech>/<port>_<tech>.{h,cpp}`:

- Implements exactly one port.
- Owns its native handle (`sqlite3*`, `asio::ip::tcp::acceptor`,
  `lua_State*`) via RAII.
- Translates native errors into `core::StatusCode`. Mapping table is
  exhaustive and unit-tested.
- Logs at DEBUG/INFO; never at ERROR for an expected
  `StatusCode::NotFound` etc.
- Configuration is passed at construction; no global lookup.

## 4. Composition root: `services/`

`src/v3/services/` is the **only** place where:

- Concrete adapters are instantiated.
- Ports are wired into use cases.
- The event loop is started.
- Plugin discovery runs (see `13-plugin-and-scripting.md`).

Pattern:

```cpp
// services/bnetd_service.h
namespace pvpgn::services {

class BnetdService final {
public:
  static core::Result<BnetdService, core::StatusCode>
      build(const config::BnetdConfig& cfg);

  void run();
  void stop() noexcept;

private:
  BnetdService(...);  // private; only build() constructs.

  // owned adapters (unique_ptrs)
  // owned use cases (value-typed, hold references)
  // event loop
};

}  // namespace
```

`app/bnetd/main.cpp` is then:

```cpp
int main(int argc, char** argv) {
  auto cfg = pvpgn::config::load_bnetd(argc, argv);
  if (!cfg) return print_and_exit(cfg.error());
  auto svc = pvpgn::services::BnetdService::build(*cfg);
  if (!svc) return print_and_exit(svc.error());
  return svc->run(), 0;
}
```

No globals. No `static` init order issues. Trivial to swap adapters
for tests (`tests/e2e/` already does this).

## 5. Enforcement

CI script `scripts/ci/check_layering.sh`:

```sh
# application must not include infra
! grep -RnE '#include[[:space:]]+["<]pvpgn/infra/' src/v3/application/
# domain must not include application/infra/protocol
! grep -RnE '#include[[:space:]]+["<]pvpgn/(application|infra|protocol)/' src/v3/domain/
# core must not include anything pvpgn-specific outside core
! grep -RnE '#include[[:space:]]+["<]pvpgn/(?!core/)' src/v3/core/
# only services may include both application and infra
grep -lRE '#include[[:space:]]+["<]pvpgn/infra/' src/v3/ | \
  grep -v '^src/v3/\(services\|infra\|integration\|app\|tests\)' && exit 1 || true
```

This becomes a required CI job in `12-build-tooling-ci.md`.

## 6. Concrete tasks

- [ ] R233 (shared with §04): publish canonical port headers.
- [ ] R247: write fakes for every port under `infra/inmemory/`.
- [ ] R248: write CI layering check.
- [ ] R249: collapse the two existing composition roots (bnetd legacy
      `main.cpp` + new `services/bnetd_service.cpp`) into a single
      v3-native composition root.
