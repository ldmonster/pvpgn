# 07 — Protocol Layer

`protocol/` is the largest surface in the tree (~22.6k LOC) and historically the
least uniformly tested. It is a **driven adapter**: it translates bytes on the
wire into application commands and back. It depends only on `core` and `domain`
and contains no business rules and no socket handling (that is `infra/net`).

## 1. Families

`bnet`, `d2cs`, `d2dbs`, `d2gs`, `d2save`, `irc`, `telnet`, `wol`,
`wolgameres`, `udp`, `file`, plus `common` for shared framing primitives.

Each family is split into three concerns (Single Responsibility):

1. **Framing** — read/write length-prefixed packets; turn a byte stream into
   discrete messages and back. Pure, no allocation surprises, fuzz-tested.
2. **Codec** — encode/decode each message type to/from typed structs. Pure
   functions; table-driven where possible.
3. **Dispatch** — map an incoming decoded message to an application use-case via
   a **registry keyed by message id** (Open/Closed: new message = new registry
   entry + handler, no growing `switch`).

## 2. Anti-corruption boundary

Wire structs are *protocol types*, not domain types. A handler:

```
bytes ──framing──▶ raw message ──codec──▶ typed wire struct
      ──translate──▶ command (domain/value objects) ──▶ use-case
      ◀──translate── result ──codec──▶ bytes
```

Wire layouts, byte order, and legacy quirks **never** leak past the translate
step. The domain never sees a `bn_int` or a packed struct.

## 3. Versioning & client quirks

- Per-client-version quirks live in small, named strategy objects selected by a
  capability/version value object — not `if (version == X)` scattered through
  codecs.
- Unknown/!malformed messages produce a typed protocol error and a controlled
  disconnect, never undefined behaviour.

## 4. Testing the protocol layer (the big lift)

This is where test coverage must rise most. For **every** family:

1. **Golden vectors.** Capture real byte sequences (from captures or
   specifications) and assert `decode(bytes) == struct` and
   `encode(struct) == bytes` (round-trip). Store under
   `tests/unit/protocol/<family>/golden/`.
2. **Round-trip property tests.** `encode(decode(x)) == x` and
   `decode(encode(s)) == s` for generated inputs.
3. **Fuzz targets.** One libFuzzer/AFL target per framing+codec entrypoint in
   `tests/fuzz/`, with a seed corpus and a locally-runnable smoke run
   (`scripts/dev/run-bench.sh`-style wrapper or a dedicated `fuzz-smoke`
   target). Fuzzers must find **no** crash/UB on the corpus under asan/ubsan.
4. **Malformed-input tests.** Truncated, oversized, and adversarial frames are
   handled with a typed error, asserted explicitly.

## 5. Tasks for this plan

1. **Normalize all families to the framing/codec/dispatch split.** Some are
   already split (`split_codec.py`, `split_handle_bnet.py`, `split_handle_wol.py`
   were one-shot tools toward this); finish and then delete those scripts.
2. **Move dispatch to registries** everywhere; remove residual mega-`switch`
   handlers.
3. **Backfill golden + round-trip + fuzz** for every family until coverage of
   `protocol/` matches `domain/`.
4. **Ensure handlers are pure adapters** — no business rules, delegate to
   use-cases (see [05-application-layer.md](05-application-layer.md)).
5. **Pin observed-on-the-wire behaviour** with the e2e suite
   ([10-e2e-and-functional.md](10-e2e-and-functional.md)) so refactors can't
   silently change bytes a real client depends on.

## Definition of Done

- [ ] Every protocol family is split into framing/codec/dispatch with a
      registry-based dispatch (no mega-switch).
- [ ] Every family has golden vectors + round-trip tests + at least one fuzz
      target; `tests/fuzz` builds and the smoke corpus runs clean under
      asan/ubsan locally.
- [ ] No protocol handler contains business rules; all delegate to use-cases.
- [ ] No wire/packed type appears in any `domain/` or `application/` signature.
