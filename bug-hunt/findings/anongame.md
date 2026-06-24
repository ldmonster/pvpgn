# Bug Hunt: anongame (WAR3 anonymous matchmaking)

ORIGINAL: `/home/cnupt/work/pvpgn-server`
V3:       `/home/cnupt/work/pvpgn`

Scope: info-reply / type composition, maplists parsing, infos parsing,
tag tables / client tags, zlib anongame framing.

---

## Finding 1 — INFOREPLY `tag_unk` echoes client value instead of fixed server constant

- Severity: HIGH
- Classification: BUG

The legacy server emits a **hardcoded per-tag** `tag_unk` value in each
INFOREPLY; it reads the client's `tag_unk` only for a debug log and never
echoes it. v3 instead copies the client request's `tag_unk` into the reply.

ORIGINAL `src/bnetd/handle_anongame.cpp` (`_client_anongame_infos`, ~L742-790):
```cpp
case CLIENT_FINDANONGAME_INFOTAG_URL:
    bn_int_set((bn_int*)&server_tag_unk, 0xBF1F1047);
    packet_append_data(rpacket, "LRU\0", 4);
    packet_append_data(rpacket, &server_tag_unk, 4);   // FIXED constant
...
case ...MAP:  server_tag_unk = 0x70E2E0D5;  "PAM\0"
case ...TYPE: server_tag_unk = 0x7C87DEEE;  "EPYT"
case ...DESC: server_tag_unk = 0xA4F0A22F;  "CSED"
case ...LADR: server_tag_unk = 0x3BADE25A;  "RDAL"
```
(`client_tag_unk` is read at L735 only for the eventlog line.)

V3 `src/application/anongame_inforeply/src/inforeply_builder.cpp:42` and `:292`:
```cpp
out.tag_unk  = tag_unk;     // tag_unk == request entry's tag_unk
```
fed from `build_inforeplies_for_request` (`e.tag_unk`, L142 / L317).

Divergence: wire byte 6..9 of every INFOREPLY differ from the real
client-expected magic numbers. Real WAR3/W3XP clients key on these
constants; echoing the request value breaks parity.

These five magic constants do not appear anywhere in v3
(`grep 0xBF1F1047|0x70E2E0D5|0x7C87DEEE|0xA4F0A22F|0x3BADE25A` → no hits).

Proposed fix: map server-reply tag → fixed `tag_unk` constant
(URL=0xBF1F1047, MAP=0x70E2E0D5, TYPE=0x7C87DEEE, DESC=0xA4F0A22F,
LADR=0x3BADE25A) inside `server_tag_for`/`compose_inforeply`, and drop the
`tag_unk` request parameter from the reply path.

---

## Finding 2 — INFOREPLY `count` echoes request count instead of fixed `1`

- Severity: MEDIUM
- Classification: BUG (UNSURE on intent — v3 doc treats it as deliberate)

ORIGINAL `handle_anongame.cpp` per-item branch (~L697):
```cpp
bn_int_set(&rpacket->u.server_findanongame_inforeply.count, 1);  // always 1
```
(The `count > 1` short-circuit branch echoes the request count, but that
is the "0 entries" early-out path, not the normal reply.)

V3 `inforeply_builder.cpp:39` / `:289`:
```cpp
out.count = count;          // count == request.count
```
`build_inforeplies_for_request` passes `request.count` to every reply
(L142 / L317), and the header explicitly documents "The `count` from the
request is propagated to every reply" (inforeply_builder.hpp:128-130).

Divergence: bytes 0..3 of each normal INFOREPLY are the request's count
value instead of the constant `1`.

Proposed fix: set `out.count = 1` for normal INFOREPLY packets.

---

## Finding 3 — DESC payload `gametype_id` uses queue index, not legacy `prefix[0]`

- Severity: HIGH
- Classification: BUG

In the legacy DESC payload, each entry's gametype byte is
`anongame_prefix[j][0]` — the *in-section gametype code* — not the queue
index `j`.

ORIGINAL `src/bnetd/anongame_infos.cpp` (`anongame_infos_data_load`, ~L1745+):
```cpp
packet_append_data(raw, &anongame_PG_section, 1);   // section id 0/1/2
packet_append_data(raw, &anongame_prefix[j][0], 1); // gametype = prefix[0]
```
where `prefix[0]` values are e.g. tffa→1, at3v3→2, 5v5→5, 6v6→6,
2v2v2→7, 3v3v3→8, 4v4v4→9, 2v2v2v2→0x0A, 3v3v3v3→0x0B (see the
`anongame_prefix[18][5]` table, L1581-1614).

V3 `src/infra/legacy_config/src/anongame_infos_loader.cpp` builds DESC from
the `kGametypes` table (L67-75) and sets `e.gametype_id = row.id` (L157),
where `row.id` is the **queue index** (tffa=6, 5v5=10, 6v6=11, 2v2v2=12,
3v3v3=13, 4v4v4=14, 2v2v2v2=15, 3v3v3v3=16). It also hardcodes
`e.section_id = 0` for every entry (L156).

Two divergences:
1. `gametype_id`: queue index vs legacy `prefix[0]`. E.g. for 5v5 v3 emits
   `10` but legacy emits `5`; for tffa v3 emits `6` but legacy emits `1`.
2. `section_id`: v3 always 0 (PG). Legacy emits the AT section (id 1) and
   TY section (id 2) too; tffa is an AT-style entry only when the prefix
   marks it, but here it is the PG queue — still the `prefix[0]` value
   diverges. Pure PG gametypes 1v1..4v4/sffa happen to have prefix[0]==
   queue index, so those 5 match by accident; everything ≥ tffa diverges.

Note the v3 `kGametypes` table only lists PG-style gametypes and never
emits AT (section 1) or TY (section 2) DESC entries that the legacy server
produces when those queues have maps. Legacy also gates DESC inclusion on
`maplists_get_totalmaps_by_queue(...)`; v3 includes a gametype whenever
both short+long strings exist regardless of maplist contents (different
inclusion criterion — see Finding 7).

Proposed fix: drive DESC `gametype_id` from the legacy prefix table's
`[0]` byte (reuse `kAnonGameDefaultPrefix`), emit per-section entries
(PG/AT/TY) matching the prefix selectors, and gate on maplist presence.

---

## Finding 4 — `type_composer` does not apply runtime thumbsdown / tournament prefix mutation

- Severity: MEDIUM
- Classification: UNSURE (documented as caller's responsibility)

ORIGINAL `anongame_infos_data_load` mutates the prefix table at build time:
- `anongame_prefix[j][2] = anongame_infos_get_thumbsdown(j);` (L1635) —
  the thumbsdown byte comes from the conf file, overwriting the static
  default in column 2.
- In the TY section (L1707-1716): `prefix[j][3] = tournament_get_races();`
  and `prefix[j][4] = tournament_is_arranged() ? tournament_get_game_type()
  : 0;`.

V3 `src/application/anongame_inforeply/src/type_composer.cpp` emits
`kAnonGameDefaultPrefix` verbatim (`g.prefix = prefix_table[j]`, L41). The
default table bakes in the static thumbsdown defaults (0x03/0x02/0x01...)
and `races=0x3F`, and never applies the conf-file thumbsdown values or the
tournament runtime values for the TY row.

Divergence: TYPE-payload prefix bytes [2] (thumbsdown) and, for TY, [3]/[4]
differ from a legacy server whose `THUMBS_DOWN_LIMIT` section or active
tournament changed them. The hpp documents that the prefix table "can be
overridden on a per-deployment basis," so this may be wired by an
un-ported caller — verify the caller actually applies thumbsdown + TY
runtime values before shipping.

---

## Finding 5 — Maplists loader adds (tag,queue,map) dedup the legacy parser lacks

- Severity: LOW
- Classification: BUG (behavioural divergence; likely the known flaky test)

The legacy `_maplists_add_map` does NOT deduplicate a map within a queue;
listing the same map twice for one `<tag,queue>` appends the index twice
(up to the 32 cap):

ORIGINAL `src/bnetd/anongame_maplists.cpp` (L86-121): no per-queue dup check —
it only dedups the *distinct mapname list* (`in_list`), then unconditionally
`maplists_war3[queue][0]++; maplists_war3[queue][n] = j;`.

V3 `src/infra/legacy_config/src/anongame_maplists_loader.cpp:138-148`:
```cpp
// Skip exact duplicate (tag,queue,map) triples.
bool already_in_queue = false;
for (auto v : queue) if (v == map_idx) { already_in_queue = true; break; }
if (already_in_queue) continue;
```

Divergence: a mapsfile listing the same `<tag,queue,map>` twice yields a
queue list with 2 indices in legacy but 1 in v3 → the MAP-info count byte
and the TYPE payload differ. (The distinct-mapname dedup itself matches.)
This is a plausible source of the "known flaky maplists test."

Proposed fix: remove the per-queue triple dedup to match legacy, OR confirm
the parity contract intentionally diverges and pin the test accordingly.

NOTE (v3 hardening, not a bug): legacy never bounds the distinct-map list
at `MAXMAPS` (100) before `maplist_war3[number_maps_war3++]`, so a long
mapsfile overflows the fixed `char *maplist_war3[100]` array. v3 guards with
`if (names.size() >= kMaplistsMaxMaps) continue;` (L128) — a safe
improvement, but it changes behaviour at >100 distinct maps (legacy would
corrupt memory; v3 drops them). Acceptable.

---

## Finding 6 — Locale section header accepted for any length, not exactly 4-char langID

- Severity: LOW
- Classification: BUG (minor parsing divergence)

ORIGINAL `anongame_infos.cpp::switch_parse_mode` (~L1267): a non-reserved
section is treated as a DESC locale **only** when `strlen(text) == 6`, i.e.
`[XXXX]` with exactly 4 chars; it copies 4 chars into `langID` and logs an
error for any other length:
```cpp
else if (std::strlen(text) == 6) {        // "[" + 4 chars + "]"
    std::strncpy(langID, &(text[1]), 4); langID[4] = '\0';
    return parse_DESC;
} else
    eventlog(... "got invalid section name: {}" ...);
```

V3 `src/infra/legacy_config/src/anongame_infos_loader.cpp::is_section_header`
(L45-49) accepts any `[...]` with length ≥ 2, and `load_…_multilocale`
treats any non-reserved section name (any length) as a langID bucket
(L258-262, L285-291).

Divergence: a malformed/odd-length section like `[ENGLISH]` or `[en]`
is silently treated as a locale in v3 but rejected (logged + ignored) by
legacy. Edge case; affects which locale buckets exist. Low impact unless a
mapsfile/infos file has stray bracket lines.

Proposed fix: restrict locale sections to exactly-4-char names to match
legacy, or accept the looser behaviour intentionally.

---

## Finding 7 — DESC entry inclusion criterion differs (string presence vs maplist presence)

- Severity: MEDIUM
- Classification: BUG

Legacy includes a gametype in the DESC payload iff that queue currently has
maps configured: every DESC loop is guarded by
`if (maplists_get_totalmaps_by_queue(game_clienttag[k], j))`
(`anongame_infos_data_load`, L1736+). The `desc_count` byte equals the
number of queues with maps.

V3 `anongame_infos_loader.cpp::build_snapshot` (L148-161) includes a
gametype iff **both** its short and long DESC strings are present
(`if (!s || !l) continue;`), independent of maplists. The DESC `count`
byte (`serialize_desc_payload`) is the number of gametypes with strings.

Divergence: DESC payload contents and the leading count byte differ from
legacy for any deployment where the set of configured gametype strings ≠
the set of queues with maps. Because DESC and TYPE are produced from
different sources in v3 (config strings vs maplists), they can disagree;
legacy keys both off the same maplist gate.

Proposed fix: gate DESC entry emission on the same maplist presence the
TYPE payload uses, so DESC and TYPE stay consistent.

---

## MATCHES (verified correct)

- **zlib framing header** — `src/infra/compression/src/zlib_anongame.cpp`
  L62-67 writes `raw_len` (LE u16) at [0..1] and `comp_len` (LE u16) at
  [2..3]. Legacy `zlib_compress` (`anongame_infos.cpp` L1965+) writes the
  same via `bn_short_set(dest, lorigdone)` + `bn_short_set(dest+2,*destlen)`,
  and `bn_short_set` is little-endian (`bn_type.cpp:397-398`). Header layout
  and endianness MATCH. (Minor: legacy deflates in 0x8000 chunks with
  Z_SYNC_FLUSH for >32KB payloads vs v3's single Z_FINISH — different
  compressed bytes for very large payloads but both inflate to identical
  data; wire-compatible.)

- **Client info-tag constants** — v3 `kAnonGameInfoTagURL/MAP/TYPE/DESC/LADR`
  (`anongame.hpp:84-88`) equal the four wire bytes of legacy
  `CLIENT_FINDANONGAME_INFOTAG_*` (`anongame_protocol.h:439-443`) read as
  LE u32 (v3) vs the numeric BE literal (legacy). Same bytes on the wire.
  Server-reply tags (`kAnonGameInfoTagServer*`, anongame.hpp:90-94) match
  the legacy `"LRU\0"/"PAM\0"/"EPYT"/"CSED"/"RDAL"` appended bytes.

- **LADR tag bytes & order** — v3 `kLadrRows` (anongame_infos_loader.cpp
  L81-92): OLOS, MAET, " AFF", 2SV2, 3SV3, 4SV4, SNLC, 2NLC, 3NLC, 4NLC,
  in that order, 10 rows; `tag_to_u32` + `write_le<u32>` reproduce the exact
  byte order of legacy `packet_append_data(raw,"OLOS",4)` … (L1718-1750).
  Count = 10 matches legacy's hardcoded `ladr_count = 10`. MATCH.

- **Maplists queue-name table** — v3 `kQueueNames` (anongame_maplists_loader
  .cpp L16-22) is identical (same 18 names, same order) to legacy
  `queue_names[ANONGAME_TYPES]` (anongame_maplists.cpp L62-67). MATCH.

- **Maplists field order / delimiters / quoting** — v3 parses
  `<clienttag(4)> <queue> <mapname|"quoted">`, strips `#` comments and
  trailing WS, requires 4-char clienttag, uppercases the tag key — all
  mirroring legacy `anongame_maplists_create` (L216-270). Distinct-mapname
  dedup and the 32-per-queue / (v3-added) 100-distinct caps match (modulo
  Finding 5). MATCH.

- **`anongame_prefix[18][5]` table** — v3 `kAnonGameDefaultPrefix`
  (type_composer.hpp L50-74) is byte-for-byte identical to legacy
  `anongame_prefix` (anongame_infos.cpp L1581-1614). MATCH.

- **TYPE section selectors** — v3 `is_pg/is_at/is_ty` (type_composer.cpp
  L18-26) use `prefix[1]`/`prefix[4]` exactly as legacy
  (`!prefix[j][1]&&!prefix[j][4]` PG, `!prefix[j][1]&&prefix[j][4]` AT,
  `prefix[j][1]` TY). Section ids 0x00/0x01/0x02 match. The per-gamestyle
  serialization (5-byte prefix + count byte + map indices) matches legacy
  `maplists_add_map_info_to_packet` (count then indices). MATCH.

- **MAP payload** — v3 `serialize_map_payload` writes count(u8) then
  NUL-terminated mapnames; legacy `mapscount_total` byte +
  `maplists_add_maps_to_packet`. MATCH.

- **ANONGAME_TYPE_* enum & queue count (18)** — v3
  `anongame_wire_types.hpp` L94-112 matches `anongame_protocol.h:448-466`.

- **INFOREPLY trailing byte** — v3 uses 0x00 for the last producing reply
  and 0x01 otherwise (inforeply_builder.cpp L140/L294, enc at
  anongame_server.cpp:230). Legacy uses `last_packet=0x00` / `other=0x01`.
  Note subtle edge: legacy compares `server_tag_count == noitems`
  (recognized count vs requested count), so if some requested tags are
  unrecognized legacy never sets 0x00 on any packet; v3 sets 0x00 on the
  last *producing* reply. Equal whenever all requested tags are recognized
  and produce data — the normal case. (Borderline; flagged for awareness,
  not counted as a separate bug.)
