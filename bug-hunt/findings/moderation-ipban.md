# Moderation / IP-ban subsystem — ORIGINAL vs v3 bug hunt

Scope: IP bans (exact / wildcard / range / netmask / prefix), account ban vs lock,
ban expiry & duration parsing, expiry-instant comparison, precedence/order of checks.

ORIGINAL refs: `pvpgn-server/src/bnetd/ipban.cpp`, `account_wrap.cpp`, `common/util.cpp`, `server.cpp`.
V3 refs: `pvpgn/src/domain/moderation/`, `pvpgn/src/application/moderation/`,
`pvpgn/src/infra/{persistence,file,inmemory}/.../ip_ban_repository.*`,
`pvpgn/src/domain/shared/include/domain/shared/ip_address.hpp`.

---

## Finding 1 — Wildcard / range / netmask / prefix IP bans are completely lost (legacy file loader silently drops them)
- **Severity:** Critical
- **Classification:** BUG
- **Original ref:** `ipban.cpp:805-897` (`ipban_str_to_ipban_entry`) builds 5 ban
  types: `ipban_type_exact`, `ipban_type_wildcard` (`1.2.3.*`), `ipban_type_range`
  (`1.2.3.4-1.2.3.40`), `ipban_type_netmask` (`1.2.3.0/255.255.255.0`),
  `ipban_type_prefix` (`1.2.3.0/24`). `ipbanlist_check` (`ipban.cpp:291-409`)
  matches all five.
- **V3 ref:** `infra/file/src/ip_ban_repository.cpp:17-44` (`load_all`) parses each
  bnban.conf line with:
  ```cpp
  auto parsed = domain::IpAddress::parse(line);
  if (!parsed.has_value()) { continue; }   // <-- silently skipped
  ```
  and `domain/shared/ip_address.hpp:79-153` (`parse_v4_`) rejects any char that is
  not a digit or `.` — so `*`, `-`, `/` all fail to parse.
- **Divergence:** Every wildcard/range/netmask/CIDR line in a legacy `bnban.conf`
  is silently `continue`d. A server that previously banned `1.2.3.*` or
  `10.0.0.0/8` will, after migrating to v3, **let all those addresses in**. No
  warning is logged (unlike the original `ipbanlist_load`, which at least logs a
  warn on bad lines). Only bare exact IPv4/IPv6 lines survive.
- **Proposed fix:** Port the original parser. In `load_all`, detect `-`, `*`, `/`
  and route to `add_ban` (exact), or compute network+prefix and call
  `add_range_ban`. Wildcards `a.b.c.*` map to `/8 /16 /24`; partial-octet
  wildcards (`1.2.*.4`) and inclusive ranges have no CIDR equivalent and need a
  dedicated matcher (see Finding 2). At minimum, log a warning on dropped lines
  instead of silently skipping.

## Finding 2 — Domain model cannot represent inclusive ranges or non-contiguous wildcards
- **Severity:** High
- **Classification:** BUG (capability regression)
- **Original ref:** `ipban.cpp:329-338` matches arbitrary inclusive ranges
  (`info1 <= ip <= info2`), and `:303-327` matches per-octet wildcards where ANY
  octet may be `*` (e.g. `1.*.3.*`, `1.2.*.4`).
- **V3 ref:** `domain/moderation/ip_ban_list.hpp:61-71,140-164` only supports
  CIDR `add_range`/`range_matches_` (contiguous power-of-two blocks aligned on a
  prefix boundary). The header comment at `:4-7` admits "CIDR ranges land in a
  follow-up commit; this first cut covers exact-host bans."
- **Divergence:** `1.2.3.4-1.2.3.40` is NOT expressible as a single CIDR, and
  `1.2.*.4` (wildcard on a middle octet) is non-contiguous — neither can be
  represented, so they can never match in v3. Match set is strictly smaller than
  the original.
- **Proposed fix:** Add a `RangeBan{lo, hi}` inclusive-range entry type and a
  per-octet `WildcardBan` type to the aggregate and matcher, mirroring
  `ipban_type_range` / `ipban_type_wildcard`. Or decompose ranges into multiple
  CIDR blocks at load time (covers ranges but not middle-octet wildcards).

## Finding 3 — `check_ip_ban` reason/expiry lookup ignores range bans entirely
- **Severity:** Medium
- **Classification:** BUG
- **Original ref:** N/A (original returns only a rule index, not a reason).
- **V3 ref:** `application/moderation/src/check_ip_ban.cpp:36-43`:
  ```cpp
  ban_repo_.for_each_entry([&](const IpBanEntry& entry) {
      if (entry.ip == ip) { ... return false; }   // exact entries only
      return true;
  });
  ```
  `for_each_entry` only walks exact entries (`inmemory` `:70-77` iterates
  `banlist_.entries()`; `persistence` `:168-176` selects only from `ip_bans`,
  never `ip_ban_ranges`).
- **Divergence:** When an IP is blocked **only** by a CIDR range, step 1
  (`is_banned`) correctly returns `true`, but the entry search in step 4 finds
  nothing, so the result is reported with `reason = "Banned"` and
  `expires_at = nullopt` (i.e. wrong reason and "permanent" even when the range
  ban has an expiry). User-facing ban message and any expiry-driven UI are wrong
  for range-matched IPs.
- **Proposed fix:** Have the repository expose range entries to the lookup (e.g.
  `for_each_range`), and in `check_ip_ban` fall back to matching ranges to
  recover the real reason/expiry.

## Finding 4 — Expiry-instant comparison inverted between prune and active_at (off-by-one at the exact expiry second)
- **Severity:** Low
- **Classification:** BUG (boundary)
- **Original ref:** `ipban.cpp:474` unloads when `(endtime - now <= 0)` i.e.
  expired once `now >= endtime` (inclusive). The check path relies on
  `unload_expired` having removed it; a still-present entry at `now == endtime`
  is treated as expired only after pruning.
- **V3 ref:** `ip_ban_list.hpp:29-31` `active_at` = `now < *expires_at`
  (so at `now == expires_at` it is INACTIVE — matches inclusive-expiry).
  But `ports.hpp:39-41` `AccountBan::active_at` = `*expires_at > now`
  (identical semantics, good). `ip_ban_list.hpp:86-107` `prune_expired` removes
  when `now >= *expires_at` (inclusive) — consistent with `active_at`. SQL paths
  use `expires_at > ?` (`persistence:85,98`), also consistent.
- **Divergence:** Internally v3 is self-consistent (expired AT the instant). The
  original is effectively the same once `unload_expired` runs. **This MATCHES** —
  noted here only to record that the boundary was checked and is correct. The
  one nit: original `ipbanlist_check` can still return a *match* for an entry in
  the window between `endtime` and the next `prune` tick (it does not re-check
  `endtime` during matching, only `unload_expired` on an interval), whereas v3
  filters on every `is_banned` call. v3 is actually *more* correct here.

## Finding 5 — Duration parsing semantics differ: original `/ipban add` value is MINUTES; v3 has no command parser and treats raw values as the source unit
- **Severity:** Medium
- **Classification:** UNSURE (v3 command/integration layer not yet wired)
- **Original ref:** `ipban.cpp:488-527` `ipbanlist_str_to_time_t`: reads the
  numeric token, runs `clockstr_to_seconds` on it, then **`return now + bmin*60`**
  — i.e. the admin-supplied number on `/ipban add <ip> <N>` is interpreted as
  **minutes** (and `N:M` colon-form as min:sec scaled). The file format
  (`ipbanlist_load`, `:159`) instead stores an absolute epoch endtime.
- **V3 ref:** `application/moderation/src/ban_ip.cpp:29-35` takes
  `req.expires_at` (an absolute `SystemTime`) directly; there is no command
  parser in `src/integration`/`src/command` for `/ipban` at all (grep found no
  wiring). `BanIp` does no duration conversion.
- **Divergence:** When the command layer is eventually wired, whoever maps the
  chat-command argument to `expires_at` must reproduce the "number = minutes"
  rule, or admins typing `/ipban add x 30` (expecting 30 minutes) will get
  30 seconds / 30 whatever. Flag for the command-layer implementer.
- **Proposed fix:** When porting the `/ipban` command, multiply the parsed bare
  number by 60 (minutes) to match operator muscle memory, and document it.

## Finding 6 — Legacy bnban.conf trailing endtime field is dropped (all file bans become permanent)
- **Severity:** High
- **Classification:** BUG
- **Original ref:** `ipban.cpp:130-171` `ipbanlist_load` splits each line into
  `ip` + optional `timestr`, converts `timestr` via `clockstr_to_seconds` to an
  `endtime`, and stores it (`ipbanlist_add(NULL, ip, endtime)`); `:219-222`
  `ipbanlist_save` writes `"<ip> <endtime>"`.
- **V3 ref:** `infra/file/src/ip_ban_repository.cpp:24-43` reads the whole line as
  one IP, never splits on whitespace, and hard-codes
  `/*expires_at*/ std::nullopt` and `reason = "Legacy ban"`.
- **Divergence:** Any timed entry in an existing `bnban.conf` (e.g.
  `1.2.3.4 1700000000`) — the line `"1.2.3.4 1700000000"` will fail
  `IpAddress::parse` (space is illegal) and be **silently dropped entirely**
  (compounds Finding 1). Even if parsing were fixed to take the first token, the
  endtime would be ignored and the ban made permanent. There is also no save-back
  of expiries.
- **Proposed fix:** Split each line on whitespace; parse token[0] as the IP and
  token[1] (if present) as the epoch endtime → `expires_at`. Mirror in any
  save path.

## Finding 7 — `BanIp` rejects re-banning an already-banned IP instead of refreshing it
- **Severity:** Low
- **Classification:** BUG (behavioral divergence)
- **Original ref:** `ipban.cpp:418-456` `ipbanlist_add` unconditionally appends a
  new entry; duplicate add of the same IP just creates a second entry (and the
  domain `IpBanList::add` at `ip_ban_list.hpp:46-51` actually de-dups by
  overwriting — closer to "refresh the expiry"). The original never errors on a
  duplicate; an admin can re-issue a ban to extend/replace its expiry.
- **V3 ref:** `application/moderation/src/ban_ip.cpp:19-26`:
  ```cpp
  if (is_banned.value()) { return core::fail(BanIpError::AlreadyBanned); }
  ```
- **Divergence:** In the original an admin re-banning an IP to change the duration
  succeeds; in v3 it is rejected with `AlreadyBanned`, so admins cannot
  extend/shorten an existing IP ban via the ban path (must unban first). Note the
  domain layer (`add` overwrites) and the SQL layer (`INSERT OR REPLACE`,
  `persistence:121`) both *support* refresh — only the application guard blocks
  it. Inconsistent with the account-ban path which has the same guard but at
  least account semantics differ.
- **Proposed fix:** Either drop the `AlreadyBanned` guard for IP bans (let the
  overwrite/refresh happen) or make it refresh-on-duplicate to match the
  original's append/overwrite behavior.

## Finding 8 — Account "lock" (auth lock, time-boxed) vs "ban" conflated / lock side not modeled
- **Severity:** Medium
- **Classification:** UNSURE
- **Original ref:** `account_wrap.cpp:396-455` models a per-account **auth lock**:
  `BNET\auth\lock` (bool) + `locktime` (auto-unlock epoch) + `lockreason` +
  `lockby`. `account_get_auth_lock` auto-clears the lock when
  `locktime - time(NULL) < 0` (lazy expiry on read). `account_get_locktext`
  (`:505-532`) renders " by X for <timestr> with reason Y" or " permanently".
  Mute is a parallel structure (`mutetime`, etc.).
- **V3 ref:** v3 has an `AccountBan` repository (`ports.hpp:31-66`) for bans, and
  `IssueWarning` (`issue_warning.cpp`) only records an audit row of action
  `AccountLocked` — it does not actually lock anything. `SilenceUser`
  (`silence_user.cpp:27-34`) is a stub: `account.silence()` is commented out and
  it saves an unchanged account. No `locktime` auto-expiry logic exists; the
  distinction between a permanent admin "ban" and a time-boxed "lock"/"mute" is
  not modeled.
- **Divergence:** The original lock/mute auto-expiry-on-read and the locktext
  message format are not reproduced. `SilenceUser` is a no-op (will not actually
  mute), and `IssueWarning` only audits. Whether this is intentional (bans
  subsume locks) or an unfinished port is unclear — flagged UNSURE.
- **Proposed fix:** Confirm intended mapping. If lock/mute are meant to live as
  `AccountBan` with `expires_at`, implement the lazy-expiry + locktext rendering;
  if separate, port the `auth\lock` / `auth\mute` state and auto-unlock.

## Finding 9 — `save_banlist` drops the ranges table; `load_banlist` drops ranges from the aggregate
- **Severity:** Medium
- **Classification:** BUG
- **Original ref:** `ipban.cpp:181-236` `ipbanlist_save` serialises ALL entry
  types (exact/wildcard/range/netmask/prefix) round-trip via
  `ipban_entry_to_str` (`:900-928`).
- **V3 ref:** `persistence:178-194` `load_banlist` selects only from `ip_bans`
  ("Exact-host entries only — the aggregate cannot carry ranges"); `:196-228`
  `save_banlist` does `DELETE FROM ip_bans` then re-inserts only the aggregate's
  exact entries, **leaving `ip_ban_ranges` untouched**. The `IpBanList` aggregate
  has `ranges_` internally but `entries()` exposes only exact (`ip_ban_list.hpp:122`).
- **Divergence:** A hot-reload / bulk-import round-trip through
  `load_banlist`→`save_banlist` silently discards all range bans from the
  in-memory snapshot (they exist in `ip_ban_ranges` SQL but not in the aggregate
  returned by `load_banlist`). Any consumer treating `load_banlist` as the full
  authoritative list under-reports active bans.
- **Proposed fix:** Either expose ranges through the aggregate (so load/save are
  lossless) or document `load_banlist` as exact-only and forbid using it for
  full snapshots.

---

## What MATCHES (verified, no divergence)
- **Expiry instant comparison**: v3 `active_at` (`now < expires_at`),
  `AccountBan::active_at` (`expires_at > now`), `prune_expired` (`now >= expires_at`),
  and SQL `expires_at > ?` are mutually consistent and equivalent to the
  original's inclusive-at-expiry behavior (Finding 4). v3 is marginally more
  correct (filters on every check, no stale-window).
- **CIDR prefix matching math**: v3 `common_prefix_match_`
  (`ip_ban_list.hpp:153-164`) and `SqlIpBanRepository::cidr_match`
  (`persistence:29-57`) correctly mask full bytes + a partial-byte mask
  `0xFF << (8 - rem)`; equivalent to the original `ipban_type_prefix`
  `lip >> (32 - prefix)` (`ipban.cpp:396-398`) for valid prefixes. (Note original
  has UB if prefix == 0: `>> 32`; v3 handles 0 correctly via `rem_bits == 0`.)
- **IPv4 host-order packing**: original `ipban_str_to_ulong` (`a<<24|b<<16|...`,
  `:944`) matches v3 `v4_packed` (`ip_address.hpp:52-58`).
- **Permanent ban representation**: original `endtime == 0` ⇔ v3
  `expires_at == nullopt`; both treated as never-expiring.
- **Precedence / order**: original checks `ipbanlist_check` at `accept()` time
  before any login handshake (`server.cpp:287`). v3 exposes `CheckIpBan` /
  `is_banned` for the same gate; the application primitive is present (wiring into
  the accept path is in the integration layer, not reviewed here — verify it is
  called pre-handshake when wired).

## Notes / lower-confidence
- Original `ipban_could_be_ip_str` has a known FIXME allowing `123.123.1*.123`
  (partial-octet wildcard) and treats `IP/-24` as a range; v3 rejects all of
  these at parse — stricter, generally safer, but a match-set difference.
- `IpAddress::parse` requires full-form IPv6 (no `::` compression,
  `ip_address.hpp:148-151`); legacy IPv6 bans written compressed won't load.
  Original used `inet_ntop` output (`server.cpp:287`) which is for IPv4 here, so
  low impact in practice but a latent gap for IPv6 bnban entries.
