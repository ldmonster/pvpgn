# Bug Hunt — Icons / Ads / Static-file serving

Subsystem: WAR3/W3XP icon table (FINDANONGAME GET_ICON / SET_ICON), HarpyWar
custom-icons (`icons.cpp`), ad banners (SID_DISPLAYAD/CLICKAD/CLICKAD2),
icons.bni serving (SID_ICONREQ 0x2D), and SID_GETFILETIME (0x33).

ORIGINAL: `/home/cnupt/work/pvpgn-server`
V3:       `/home/cnupt/work/pvpgn`

Legend for classification: BUG / INTENTIONAL / UNSURE / NOT-IMPLEMENTED / MATCHES.

---

## Summary of the lay of the land

The v3 rewrite has split this subsystem into **pure builder/dispatcher
functions** plus **wire codecs**, but for the live request path the FSM
handlers are all **no-ops** that return `core::ok()` without sending any reply.
The pure functions (`build_icon_reply_table`, `dispatch_ad_pick`,
`dispatch_ad_click`) and the encoders (`enc_icon_reply`, `encode(IconReply)`,
the ad-reply codecs) exist and are unit-tested, but **nothing calls them from a
handler**. So the most severe issues are "implemented-but-not-wired" gaps, plus
a couple of genuine logic bugs in the pure functions themselves.

Icon **tier table values** were verified and **MATCH** (see Finding 7).

---

## Finding 1 — Ad banner request (SID_DISPLAYAD) handler is a no-op; no ADREPLY ever sent

- Severity: HIGH
- Classification: NOT-IMPLEMENTED (regression vs original)

Original ref — `src/bnetd/handle_bnet.cpp:3351` `_client_adreq`:
```cpp
const AdBanner* ad = AdBannerList.pick(conn_get_clienttag(c), conn_get_gamelang(c),
                                       bn_int_get(packet->u.client_adreq.prev_adid));
if (!ad) return 0;
...
packet_set_type(rpacket, SERVER_ADREPLY);
bn_int_set(&rpacket->u.server_adreply.adid, ad->get_id());
bn_int_set(&rpacket->u.server_adreply.extensiontag, ad->get_extension_tag());
file_to_mod_time(c, ad->get_filename().c_str(), &rpacket->u.server_adreply.timestamp);
packet_append_string(rpacket, ad->get_filename().c_str());
packet_append_string(rpacket, ad->get_url().c_str());
conn_push_outqueue(c, rpacket);
```

V3 ref — `src/protocol/bnet/src/fsm/fsm_misc.cpp:48`:
```cpp
core::Status<> BnetFsm::on(const AdRequest&) {
    // Banner fetches are advisory; ...
    return core::ok();
}
```

Divergence: original picks an ad and replies with adid / extensiontag /
timestamp / filename / url. v3 silently drops the request — no banner is ever
served. `dispatch_ad_pick` (the pure replacement for `AdBannerSelector::pick`)
is implemented and tested but never invoked from this handler.

Proposed fix: wire `on(AdRequest)` to resolve candidates from the ads
repository, call `dispatch_ad_pick`, and (when `chosen`) emit the SERVER_ADREPLY
via the existing ad-reply encoder.

---

## Finding 2 — SID_CLICKAD2 (and CLICKAD) handler is a no-op; no click reply / URL sent

- Severity: MEDIUM
- Classification: NOT-IMPLEMENTED (regression vs original)

Original ref — `src/bnetd/handle_bnet.cpp:3419` `_client_adclick2`:
```cpp
const AdBanner* const ad = AdBannerList.find(conn_get_clienttag(c),
        conn_get_gamelang(c), bn_int_get(packet->u.client_adclick2.adid));
if (!ad) return 0;
packet_set_type(rpacket, SERVER_ADCLICKREPLY2);
bn_int_set(&rpacket->u.server_adclickreply2.adid, ad->get_id());
packet_append_string(rpacket, ad->get_url().c_str());
conn_push_outqueue(c, rpacket);
```
(`_client_adclick` at :3407 only logs; `_client_adack` at :3389 is a true no-op —
those two match v3's no-op behaviour.)

V3 ref — `src/protocol/bnet/src/fsm/fsm_misc.cpp:54,62`:
```cpp
core::Status<> BnetFsm::on(const AdClick&)         { return core::ok(); }
core::Status<> BnetFsm::on(const AdClick2Request&) { return core::ok(); }
```

Divergence: original CLICKAD2 returns the banner URL (so the client opens the
browser). v3 drops it. `dispatch_ad_click` exists (pure) but is never called.

Proposed fix: wire `on(AdClick2Request)` to call `dispatch_ad_click` and emit
SERVER_ADCLICKREPLY2 with the URL.

---

## Finding 3 — `dispatch_ad_click` ignores language; original `find` requires lang match

- Severity: LOW
- Classification: BUG

Original ref — `src/bnetd/adbanner.cpp:243` `AdBannerSelector::find`:
```cpp
return (a.get_client() == client_tag || a.get_client() == 0)
    && (a.get_language() == client_lang || a.get_language() == 0)   // <-- lang checked
    && a.get_id() == ad_id;
```

V3 ref — `src/application/ads/src/ad_pick.cpp:88`:
```cpp
auto it = std::find_if(known.begin(), known.end(),
    [&](AdCandidate const& a) {
        return a.id == req.ad_id
            && (a.client_tag == 0 || a.client_tag == req.client_tag);  // no lang check
    });
```

Divergence: `dispatch_ad_click` only filters on `ad_id` + `client_tag`; it never
considers `lang_tag`. With ids unique this rarely matters, but it diverges from
the original `find` contract and `AdClickRequest` has no `lang_tag` field at all.

Proposed fix: add `lang_tag` to `AdClickRequest` and apply the same
`(a.lang_tag == 0 || a.lang_tag == req.lang_tag)` predicate as `find`.

---

## Finding 4 — `dispatch_ad_pick` single-candidate path: tag/lang filter applied, original returns banner[0] unconditionally

- Severity: LOW
- Classification: UNSURE (behavioural divergence; likely benign/arguably an improvement)

Original ref — `src/bnetd/adbanner.cpp:156`:
```cpp
switch (this->m_banners.size()) {
case 0:  return nullptr;
case 1:  return &this->m_banners.at(0);   // <-- NO client/lang/ext filter at all
default: { ... filter ... }
}
```

V3 ref — `src/application/ads/src/ad_pick.cpp:42`:
```cpp
if (req.candidates.empty()) return out;          // size 0 -> none  (OK)
std::vector<AdCandidate> filtered;               // always filters by tag/lang
...
if (filtered.empty()) return out;
if (filtered.size() == 1) { out.chosen = filtered.front(); return out; }
```

Divergence: when exactly **one** banner is configured, the original returns it to
*every* client regardless of clienttag/lang/extension. v3 filters first, so a
single non-matching banner yields "no ad". Behaviour also differs for the
extension-tag (MNG vs non-MNG) filter, which the original applies only in the
`default:` (>1) branch and which v3 defers to the bridge entirely
(`ad_pick.cpp` header note). Flag as UNSURE: v3's behaviour is arguably more
correct, but it is a deliberate divergence from the original wire behaviour and
should be confirmed against real client expectations.

Proposed fix: if exact parity is desired, special-case
`req.candidates.size() == 1` to return that candidate before filtering.

---

## Finding 5 — Extension-tag (MNG vs non-MNG) filtering dropped from the application layer

- Severity: MEDIUM
- Classification: NOT-IMPLEMENTED (deferred to "legacy bridge" that does not exist yet)

Original ref — `src/bnetd/adbanner.cpp:174-192`: WAR3/W3XP must use **MNG**
banners only; all other games must use **non-MNG** banners. A candidate failing
this is excluded:
```cpp
if (client_tag == CLIENTTAG_WAR3XP_UINT || client_tag == CLIENTTAG_WARCRAFT3_UINT) {
    if (bn_int_tag_eq(ext, EXTENSIONTAG_MNG) != 0) return;   // skip non-MNG for WC3
} else {
    if (bn_int_tag_eq(ext, EXTENSIONTAG_MNG) == 0) return;   // skip MNG for others
}
```

V3 ref — `src/application/ads/include/application/ads/ad_pick.hpp:19-21` (header
comment) and `ad_pick.cpp` `matches()` (no extension filter). `AdCandidate` has
an `extension_tag` field but `dispatch_ad_pick` never consults it.

Divergence: WC3 clients can be handed a non-MNG (e.g. PCX/SMK) banner and vice
versa, which the original explicitly forbids. The comment says this is delegated
to the "legacy bridge", but no bridge wires this path (Finding 1), so it is
currently unimplemented end-to-end.

Proposed fix: either implement the extension filter inside `dispatch_ad_pick`
(pass the MNG/non-MNG decision in `AdPickRequest`), or ensure the bridge applies
it before building `candidates`.

---

## Finding 6 — SID_ICONREQ (0x2D, icons.bni metadata) handler is a no-op; no ICONREPLY sent

- Severity: HIGH
- Classification: NOT-IMPLEMENTED (regression vs original)

Original ref — `src/bnetd/handle_bnet.cpp:1270` `_client_iconreq`:
```cpp
packet_set_type(rpacket, SERVER_ICONREPLY);
file_to_mod_time(c, prefs_get_iconfile(), &rpacket->u.server_iconreply.timestamp);
if ((conn_get_clienttag(c) == CLIENTTAG_WARCRAFT3_UINT) ||
    (conn_get_clienttag(c) == CLIENTTAG_WAR3XP_UINT))
    packet_append_string(rpacket, prefs_get_war3_iconfile());   // war3 icon BNI
else
    packet_append_string(rpacket, prefs_get_iconfile());        // icons.bni
conn_push_outqueue(c, rpacket);
```

V3 ref — `src/protocol/bnet/src/fsm/fsm_misc.cpp:77`:
```cpp
core::Status<> BnetFsm::on(const IconRequest&) {
    // Icons.bni metadata fetch is part of the early handshake.
    return core::ok();
}
```

Divergence: original replies with the timestamp + the correct BNI filename
(WAR3/W3XP get the war3 icon file, everyone else `icons.bni`). v3 drops it. The
`IconReply` message (`messages_misc.hpp:185`) and its encoder
(`encode(Writer&, const IconReply&)`, `codec.hpp:91`) are implemented but
unused.

Also note: v3 config (`infra/config/server_config.hpp:128`) only has
`iconfile = "icons.bni"` — there is **no `war3_iconfile`** equivalent, so even
once wired, the WAR3/W3XP-specific BNI ("the client wants a different file for
WAR3/W3XP" — original comment line 1284) cannot be served. That is a missing
config field on top of the missing handler.

Proposed fix: implement `on(IconRequest)` to build an `IconReply` (timestamp via
file mod-time of the configured icon file, filename chosen by clienttag), and
add a `war3_iconfile` config option.

---

## Finding 7 — Icon tier (race-win threshold) table — VALUES MATCH

- Severity: n/a
- Classification: MATCHES

This is the table the earlier ladder hunt flagged (the "1500 win tier").

Original defaults — `src/bnetd/anongame_infos.cpp:171-186`
(`anongame_infos_ICON_REQ_init`):

| Series           | Levels                          |
|------------------|---------------------------------|
| WAR3 race wins   | `25, 250, 500, 1500` (4 levels) |
| W3XP race wins   | `25, 150, 350, 750, 1500` (5)   |
| Tourney wins     | `10, 75, 150, 250, 500` (5)     |

V3 default-less loader — `src/infra/legacy_config/src/icon_req_loader.cpp`
parses `[ICON_REQUIRED_RACE_WINS_WAR3]` / `[..._W3XP]` /
`[ICON_REQUIRED_TOURNEY_WINS]` with `LevelN = value`, into
`IconReqTable{war3[4], w3xp[5], tourney[5]}`
(`icon_table.hpp:35-40, 28-30`). The array sizes and section→array mapping line
up exactly with the original member layout.

V3 `check_user_icon` equivalent — `validate_user_icon`
(`icon_table.cpp:100-137`) hard-codes the W3XP series for the non-tourney
columns and the tourney series for the 'D' column, exactly mirroring the
original `check_user_icon` (`handle_anongame.cpp:634-635`):
```cpp
int icon_req_wins[5]         = { 25, 150, 350, 750, 1500 };
int icon_req_wins_tourney[5] = { 10,  75, 150, 250,  500 };
```
and the default `"1O3W"` (letter O) always-valid case matches
(`handle_anongame.cpp:584` / `icon_table.cpp:107`). MATCH.

⚠️ One caveat below (Finding 8): the v3 **loader has no built-in defaults**, so
the values only match if the config file is present and well-formed.

---

## Finding 8 — Icon req loader has no built-in defaults (zero-filled → all icons unlocked)

- Severity: MEDIUM
- Classification: BUG (latent, depends on config presence)

Original ref — `src/bnetd/anongame_infos.cpp:159` `anongame_infos_ICON_REQ_init`
unconditionally seeds all 14 thresholds with the hard-coded defaults *before*
any config override.

V3 ref — `src/infra/legacy_config/src/icon_req_loader.cpp:48-56`:
```cpp
IconReqTable out{};      // war3{}=0, w3xp{}=0, tourney{}=0  -> ALL ZERO
...                      // only keys present in the file are filled
return out;
```
`IconReqTable` members are `std::array<…>{}` (`icon_table.hpp:36-38`), i.e.
zero-initialised.

Divergence: if the config file is missing a section/level, or the file fails to
load, the corresponding threshold stays **0**. In `build_icon_reply_table`
(`icon_table.cpp:91`) `wins >= threshold` with `threshold == 0` is always true →
**every icon shows as unlocked**, and `validate_user_icon` accepts any icon →
the "ICON SWITCH hack protection" the original provides is defeated. The original
never has a zero-default because `_init` always runs.

Proposed fix: initialise `IconReqTable out` with the original defaults
(`{25,250,500,1500}`, `{25,150,350,750,1500}`, `{10,75,150,250,500}`) before
applying file overrides, or have the caller fall back to a defaulted table on
load failure.

---

## Finding 9 — FINDANONGAME GET_ICON reply builder (`build_icon_reply_table`) — logic review

- Severity: n/a (builder is correct) / see wiring note
- Classification: MATCHES (builder) + NOT-IMPLEMENTED (wiring)

Original ref — `src/bnetd/handle_anongame.cpp:448` `_client_anongame_get_icon`.
V3 ref — `src/application/icon_table/src/icon_table.cpp:48` `build_icon_reply_table`.

Verified matching behaviour:
- Table dims: WAR3 → 5×4, else 6×5 (`icon_table.cpp:56-57` vs
  `handle_anongame.cpp:469-486`). MATCH.
- `curricon`: user_icon overrides default `"%1d%c3W"` (`default_user_icon`,
  matches `handle_anongame.cpp:506-508`); custom_icon overrides only when no
  user_icon (`icon_table.cpp:61-66` vs `handle_anongame.cpp:512-521`). MATCH.
- `assignedCustomIcon` forces `client_enabled = 0` for every cell
  (`icon_table.cpp:68,91` vs `handle_anongame.cpp:513,538/548`). MATCH.
- Per-cell: race columns use `req.war3/req.w3xp[row]`; the i==5 column (W3XP
  only) uses `req.tourney[row]`; both compare against `race_wins[i]` where i==5
  maps to DEMONS — matches original using `race[i]` (=W3_RACE_DEMONS at i==5)
  in both branches (`handle_anongame.cpp:538,548`). MATCH.
- icon_code = `{icon_pos[j], raceChar[i], '3', 'W'}`, `required_wins`
  serialised big-endian (`enc_icon_reply` `write_be<u16>`,
  `anongame_server.cpp:257`) matching original `bn_short_set`. MATCH.
- `table_size = width*height` byte present in `enc_icon_reply`
  (`anongame_server.cpp:250`) matching original
  `bn_byte_set(table_size, table_width*table_height)`
  (`handle_anongame.cpp:524`). MATCH.

Wiring gap: there is **no FSM `on(AnonGameGetIcon)` handler** that calls
`build_icon_reply_table` and sends `AnonGameIconReply`. The decoder
(`dec_get_icon`) and encoder (`enc_icon_reply`) exist, but the live request is
not answered (no `on(...)` found for the anongame GET_ICON sub-option that
builds the reply). Classify the end-to-end path as NOT-IMPLEMENTED.

---

## Finding 10 — SID_GETFILETIME (0x33) handler is a no-op; no FILEINFOREPLY sent

- Severity: MEDIUM
- Classification: NOT-IMPLEMENTED (regression vs original)

Original ref — `src/bnetd/handle_bnet.cpp` `_client_fileinforeq` (around line
1460) builds a SERVER_FILEINFOREPLY with `file_to_mod_time(c, filename, …)` so
the client can decide whether to re-download (autoupdate / icons / TOS files).

V3 ref — `src/protocol/bnet/src/fsm/fsm_auth.cpp:182`:
```cpp
core::Status<> BnetFsm::on(const FileInfoRequest&) {
    // GETFILETIME may be sent ... accept in any non-Closing state.
    return core::ok();
}
```

Divergence: original returns file timestamp + name; v3 drops it. The
`FileInfoReply` message type exists (`messages_game.hpp:71` region) and the
codec encodes GETFILETIME (`codec_auth.cpp:334`), but the handler never produces
a reply. Clients relying on GETFILETIME for autoupdate/icon-file freshness will
not receive timestamps.

Proposed fix: implement `on(FileInfoRequest)` to resolve the requested file's
mod-time and emit FILEINFOREPLY.

---

## Cross-cutting note

The pattern across Findings 1, 2, 6, 9, 10 is identical: **codec + pure
builder present and tested, FSM handler stubbed to `core::ok()`**. If these are
intended to be wired by a not-yet-written "legacy bridge", they are tracked
work; if the rewrite is meant to be feature-complete, these are user-visible
regressions (no banners, no banner click-through, no icons.bni serving, no
anongame icon grid, no file-time freshness). The genuine *logic* bugs that would
survive wiring are Findings 3 (click lang filter), 5 (MNG ext filter), and 8
(zero-default thresholds defeating icon-switch protection).
