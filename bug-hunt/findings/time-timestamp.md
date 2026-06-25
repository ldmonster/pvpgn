# Bug-hunt: time / timestamp / clock handling

Subsystem: FILETIME conversion, unix-time storage, tzbias, game uptime.
ORIGINAL: `/home/cnupt/work/pvpgn-server`  CURRENT v3: `/home/cnupt/work/pvpgn`

## Summary of severity

| # | Severity | Class | One-line |
|---|----------|-------|----------|
| 1 | **High** | BUG | v3 runtime uses NON-legacy account-attribute keys (`createtime`/`lastlogin`) where original uses `ctime`/`lastlogin_time` — read/write mismatch, data loss vs legacy files |
| 2 | **High** | BUG | v3 migration tool reads create time from `BNET\acct\created`, a key the original NEVER writes (original = `BNET\acct\ctime`) — every migrated account gets createtime 0 |
| 3 | Medium | UNSURE | New integer `core::bnettime.hpp` is byte-divergent from legacy float `secs_to_bnettime`; harmless today (unused on the wire) but a latent regression when wired up |
| 4 | Low | INTENTIONAL/incomplete | SID_AUTH_INFO `mpq_filetime` hardcoded to 8 zero bytes in v3 FSM (stub, not yet wired to bnettime) |
| 5 | Info | MATCH | FILETIME epoch constant, tzbias signedness, local_tzbias, bn_long byte order all verified equivalent |

---

## Finding 1 — Account time attribute keys diverge (HIGH, BUG)

v3's runtime `AttributeMap` accessors use attribute keys that do not match the
original PvPGN account-file keys. An account written by v3 and read by legacy
(or vice-versa), or any code that expects the legacy key, will see the field as
absent.

**Original** `src/bnetd/account.cpp:167` (create time) and
`src/bnetd/account_wrap.cpp:605-618` (lastlogin):
```cpp
account_set_numattr(account, "BNET\\acct\\ctime", (unsigned int)now);   // create time
...
account_get_numattr(account, "BNET\\acct\\ctime");                      // account_get_ll_ctime
account_get_numattr(account, "BNET\\acct\\lastlogin_time");             // get
account_set_numattr(account, "BNET\\acct\\lastlogin_time", t);          // set
```

**v3** `src/domain/identity/src/attribute_map.cpp:42-88`:
```cpp
get("BNET\\acct\\lastlogin")     // last_login()   -- legacy key is lastlogin_time
get("BNET\\acct\\createtime")    // created_at()   -- legacy key is ctime
set("BNET\\acct\\lastlogin",  std::to_string(timestamp));
set("BNET\\acct\\createtime", std::to_string(timestamp));
```

Divergence: `createtime` vs `ctime`, `lastlogin` vs `lastlogin_time`.
The unit (unix seconds, UTC) and 64-bit `std::stoll` parsing are fine; the
problem is the **key name**. Note v3's own tooling proves the legacy name:
`src/tools/client/bnstat_v3.cpp:178` and
`src/app/pvpgn-migrate/migrate_accounts.cpp:313` both reference
`"BNET\\acct\\lastlogin_time"`.

Proposed fix: change the runtime accessors to the legacy keys
`BNET\acct\ctime` and `BNET\acct\lastlogin_time` (or add a compatibility
read-fallback). If v3 deliberately re-keys the storage schema, the migration
tool and all readers must agree — currently they do not (see Finding 2).

---

## Finding 2 — Migration reads a create-time key that never existed (HIGH, BUG)

**v3** `src/app/pvpgn-migrate/migrate_accounts.cpp:309-312`:
```cpp
const auto created_at =
    pvpgn::infra::file::get_numeric_field(kv, {"BNET", "acct", "created"});
const auto last_login =
    pvpgn::infra::file::get_numeric_field(kv, {"BNET", "acct", "lastlogin_time"});
```

The original writes create time **only** under `BNET\acct\ctime`
(`account.cpp:167`); there is no `BNET\acct\created` attribute anywhere in the
original (`grep 'acct\\created' src/bnetd` → no hits). So the migrator's
`created_at` resolves to the field's default (0/empty) for every legacy
account, and the emitted TOML `created_at = 0`. `last_login` is correct.

Proposed fix: read `{"BNET","acct","ctime"}` for the create timestamp.

---

## Finding 3 — Two bnettime implementations; integer port is byte-divergent (MEDIUM, UNSURE)

v3 carries BOTH:
- legacy float port `src/core/time/bnettime.cpp` (faithful 1:1 of the original,
  same `UNIX_EPOCH 11644473600.`, same `UPPER_PER_SEC`/`LOWER_PER_SEC`, same
  `secs_to_bnettime` computing `temp.u` and `temp.l` independently).
- new integer `src/core/include/core/bnettime.hpp` (`to_bnettime`,
  `BnetTime`, `kUnixEpochOffsetUnits = 116'444'736'000'000'000ULL`).

The two are **not bit-identical**. The original/legacy `secs_to_bnettime`
(`bnettime.cpp:90`) sets the two 32-bit halves from independent truncated
products:
```cpp
temp.u = (unsigned int)(secs*UPPER_PER_SEC);   // 10E7/2^32
temp.l = (unsigned int)(secs*LOWER_PER_SEC);   // 10000000.
```
i.e. `u` is NOT `floor(total_units / 2^32)` and `l` is NOT
`total_units mod 2^32`; `l` is `floor(secs*1e7) mod 2^32` only by float
happenstance, and `u`/`l` carry independent rounding. The new `.hpp` computes a
single `uint64 units` and splits it cleanly via `from_u64` (the mathematically
correct FILETIME). For a given timestamp the low word from the two methods can
differ by a few units and the high-word/low-word carry boundary differs.

Severity is MEDIUM-but-latent because **`core/bnettime.hpp` is currently
included by NOTHING in `src/`** (only its own intent) — the wire code does not
use it yet. When the SID_AUTH_INFO / profile lastgame / BNFTP paths get wired
up, choosing the integer version will produce different bytes than legacy
PvPGN. Battle.net clients tolerate this (it's only displayed time), but
cross-checking against captured packets / the legacy server will mismatch.

Classification UNSURE: the integer version is arguably *more* correct; flagging
because it is an intentional behavioral change from the original and there is no
test asserting byte-equivalence with the legacy path. There is no
`*bnettime*test*` in the tree.

Proposed action: pick ONE implementation for the wire, delete the other, and
add a golden-vector test against the canonical pairs in the header comment
(e.g. `"29304451 3046165090"` ↔ 11/04/99 05:16 GMT) so the chosen math is
pinned.

---

## Finding 4 — SID_AUTH_INFO mpq_filetime stubbed to zero (LOW, INTENTIONAL/incomplete)

**v3** `src/application/connection/src/connection_fsm_connecting.cpp:60,68-69`:
```cpp
//   [12..19] mpq_filetime  (8 bytes, 0 for now)
...
// mpq_filetime (8 bytes, zero)
for (int i = 0; i < 8; ++i) reply.push_back(std::byte{0});
```
server_token is also `0xDEADBEEF` placeholder. The original fills the MPQ
filetime from the MPQ's `st_mtime` via `time_to_bnettime` +
`bnettime_to_bn_long` (cf. `src/bnetd/file.cpp:152-153`). This is an
acknowledged stub ("for now"), not a numeric bug, but it means the version-check
MPQ filetime field is presently meaningless. No epoch/unit error to fix — just
incomplete. Track for the milestone that wires CheckRevision.

---

## Finding 5 — Things that MATCH (verified correct)

- **FILETIME epoch constant — CORRECT.** Original `bnettime.cpp:81`
  `UNIX_EPOCH 11644473600.` seconds. v3 legacy `core/time/bnettime.cpp:79` same.
  v3 integer `core/bnettime.hpp:59,64`:
  `kBnetTimeUnitsPerSec = 10'000'000`,
  `kUnixEpochOffsetUnits = 116'444'736'000'000'000ULL`
  = 11644473600 × 10^7. Both the seconds form and the 100-ns form are the
  canonical Windows FILETIME offset. No wrong-epoch / off-by-one.

- **bnettime unit = 100 ns (1e-7 s) — CORRECT** in both v3 impls.

- **tzbias signedness — CORRECT.** Original parses tzbias as `unsigned int`
  then `uint32_to_int(tzbias)` (`handle_bnet.cpp:511,566`). v3 decode
  `codec_legacy_ols.cpp:93-95` reads `uint32` then
  `static_cast<std::int32_t>(...)` — equivalent two's-complement reinterpret.
  `uint32_to_int` still exists in v3 at `src/core/types/bn_type.cpp:649`.

- **local_tzbias — CORRECT/equivalent.** Original `bnettime.cpp:190-212` and
  v3 legacy `core/time/bnettime.cpp:185-207` are identical (same
  gmtime/localtime/mktime diff, same `west=positive / east=negative`,
  `/60` minutes). v3 integer `core/bnettime.hpp:160-185`
  (`local_tzbias_minutes`) reproduces the same convention with `_r` reentrant
  variants. Note the original's `-(int)(testloc - test) / 60` relies on
  `(int)` cast before negate; v3's wording is equivalent. Not wired on the wire
  yet, so no live divergence.

- **bnettime_add_tzbias direction — CORRECT.** Original subtracts
  `tzbias*60` seconds (`bnettime.cpp:217`); v3 legacy same
  (`bnettime.cpp:212`); v3 integer subtracts `bias_minutes*60*units`
  (`bnettime.hpp:191-198`) — same sign.

- **bn_long wire byte order — CORRECT.** Original `bn_long_set_a_b`
  (`bn_type.cpp:487`) emits `srcb` (lower) bytes first LE, then `srca` (upper)
  LE — i.e. the full 64-bit FILETIME little-endian. v3 profile_reply copies the
  precomputed 8 bytes verbatim (`profile_reply.cpp:113-115`), with the comment
  "legacy emits via bnettime_to_bn_long already in LE" — consistent, provided
  the producer of `lastgame_bn_long` used the legacy `bnettime_to_bn_long`
  (which it must, to match).

- **Account time width — no Y2038 issue.** v3 stores/parses with
  `std::stoll` / `std::to_string` over `std::chrono::seconds` (64-bit), so the
  string storage is 64-bit-safe. The original truncates create time to
  `(unsigned int)now` at write (`account.cpp:167`) — that is the *original's*
  32-bit foible; v3 is actually wider here (not a regression).

- **Game uptime / elapsed seconds.** Original's gamelist reply
  (`handle_bnet.cpp:3747-3817`) carries fixed `unknown4/5/6` constants and no
  live elapsed-seconds field in that path; `game_get_create_time`
  (`game.cpp:2068`) returns a `std::time_t`. v3 keeps `created_at` as
  `core::SystemTime` (`game_snapshot.hpp:27`). v3's gamelist wire path is not
  built yet, so nothing to diverge. No unit/epoch bug found here.

---

## Notes / loose ends for follow-up

- No `bnettime`-byte-equivalence unit test exists in v3. Recommend pinning the
  header's own golden pairs before either bnettime impl is wired to the wire.
- Decide the canonical create/lastlogin attribute schema (Findings 1+2): the
  runtime accessor, the migrator, and `bnstat_v3` currently reference three
  different key spellings (`createtime`/`created`/`ctime`,
  `lastlogin`/`lastlogin_time`).
