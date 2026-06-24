# Bug Hunt — Client Identification / Locale / Connection Init

Subsystem: client tags, SID_CLIENTID/CLIENTID2 (PROGIDENT/PROGIDENT2),
SID_AUTH_INFO, COUNTRYINFO1, init connection-class byte, country/locale.

- ORIGINAL: `/home/cnupt/work/pvpgn-server`
- CURRENT v3: `/home/cnupt/work/pvpgn`

## Summary verdict

The static constants (every 4-byte client/arch/gamelang tag, the init
connection-class bytes, the auth/init packet type codes) are **byte-for-byte
correct**. The init-class dispatch logic is a faithful port. The real
divergences are all in the *new FSM auth path* (`fsm_auth.cpp`), which is a
partial reimplementation that drops policy + side-effects the legacy handler
performed. Most are NOT-IMPLEMENTED (rewrite still in progress), but two are
behavioral regressions worth flagging.

---

## VERIFIED MATCHES (coverage)

### Client tag constants (all MATCH)
v3 `src/domain/shared/include/domain/shared/client_tag.hpp` vs original
`src/common/tag.h`. Verified the packed-BE uint of every tag:

| tag  | uint        | tag  | uint        | tag  | uint        |
|------|-------------|------|-------------|------|-------------|
| CHAT | 0x43484154  | STAR | 0x53544152  | SEXP | 0x53455850  |
| SSHR | 0x53534852  | DRTL | 0x4452544C  | DSHR | 0x44534852  |
| W2BN | 0x5732424E  | D2DV | 0x44324456  | JSTR | 0x4A535452  |
| D2ST | 0x44325354  | D2XP | 0x44325850  | WAR3 | 0x57415233  |
| W3XP | 0x57335850  | IIRC | 0x49495243  | WCHT | 0x57434854  |
| TSUN | 0x5453554E  | TSXP | 0x54535850  | RALT | 0x52414C54  |
| RAL2 | 0x52414C32  | DN2K | 0x444E324B  | NOXX | 0x4E4F5858  |
| NOXQ | 0x4E4F5851  | RNGD | 0x524E4744  | RFDS | 0x52464453  |
| YURI | 0x59555249  | EBFD | 0x45424644  | LOR3 | 0x4C4F5233  |
| WWOL | 0x57574F4C  | UNKN | 0x554E4B4E  |      |             |

Arch tags MATCH: IX86=0x49583836, PMAC=0x504D4143, XMAC=0x584D4143.
Gamelang tags MATCH: enUS=0x656E5553, deDE=0x64654445, csCZ=0x6373435A,
esES=0x65734553, frFR=0x66724652, itIT=0x69744954, jaJA=0x6A614A41,
koKR=0x6B6F4B52, plPL=0x706C504C, ruRU=0x72755255, zhCN=0x7A68434E,
zhTW=0x7A685457.

`is_valid_client()` / `is_valid_arch()` / `is_wol_v1()` / `is_wol_v2()` /
`title()` mirror legacy `tag_check_client/arch/wolv1/wolv2/clienttag_get_title`
member-for-member (cross-checked `tag.cpp`). `tag_wol_locale_*` enum (38
values) in `src/core/types/tag.h` is identical to original.

NB: legacy `src/core/types/tag.h` + `tag_clienttag.cpp` + `tag_wol.cpp`
(with `tag_check_in_list`, `tag_sku_to_uint`, `tag_channeltype_to_uint`,
`tag_wol_locale_to_uint`, `tag_validate_client`) are carried verbatim as a
bridge alongside the new `ClientTag` class.

### Init connection-class bytes (all MATCH)
v3 `src/protocol/bnet/include/protocol/bnet/init_wire_types.hpp` vs original
`src/common/init_protocol.h`:

| name        | byte | name           | byte |
|-------------|------|----------------|------|
| BNET        | 0x01 | FILE (BNFTP)   | 0x02 |
| BOT         | 0x03 | ENC            | 0x04 |
| TELNET      | 0x0d | D2CS           | 0x01 |
| D2GS        | 0x64 | D2CS_BNETD     | 0x65 |
| LOCALMACHINE| 0x98 |                |      |

NOTE on the task brief: the brief described the mapping as
"0x01 telnet / 0x02 bnftp / 0x03 bnet / 0x04 bot". That is NOT the original
mapping. Original (and v3) is BNET=0x01, FILE/BNFTP=0x02, BOT=0x03, ENC=0x04,
TELNET=0x0d. v3 matches the real original; the brief's framing was wrong, no
bug here.

### Init-class dispatch logic (MATCH)
`src/application/init/include/application/init/init_conn_dispatch.hpp`
faithfully reproduces `src/bnetd/handle_init.cpp`:
- per-IP cap uses strict `conn_count > max` (legacy: `connlist_count > max`) ✓
- D2CS_BNETD exempted from the cap ✓
- D2CS_BNETD realmlist gate ✓
- ENC + LOCALMACHINE → reject ✓ (legacy returns -1 for both)
- unknown byte → reject ✓

### Auth/init packet type codes (MATCH)
`auth_wire_types.hpp`: CompInfo1=0x05ff, CompInfo2=0x1eff, CompReply=0x05ff,
SessionKey1=0x28ff, SessionKey2=0x1dff, CountryInfo1=0x12ff, AuthInfo=0x50ff,
ProgIdent=0x06ff, AuthReq1(server)=0x06ff, AuthReq109(server)=0x50ff,
AuthReq1(client)=0x07ff, AuthReply1=0x07ff, AuthReply109/AuthReq109=0x51ff,
RegSnoop=0x18ff, IconReq=0x2dff. ProgIdent2=0x0bff (used in legacy codec).
CompInfo magic fields (0x00000001 / 0xaa8843d1 / 0x001b9dda / 0xab69f79a) and
RegSnoop HKEYs MATCH.

### Wire field layouts (MATCH)
- **AUTH_INFO (0x50ff)** `decode_auth_info` in
  `src/protocol/bnet/src/codec/codec_auth.cpp:16` reads, in order:
  protocol_id, platform_id(=archtag), game_id(=clienttag), version_id,
  language_id(=gamelang), local_ip, tz_bias, mpq_locale(=lcid), lang_id(=langid),
  country_abbr(=langstr str), country(=countryname str). This is exactly
  `t_client_auth_info` (bnet_protocol.h:597-611). Field order is correct;
  only the names are remapped. ✓
- **COUNTRYINFO1 (0x12ff)** `decode_countryinfo1`
  (`codec_legacy_ols.cpp:87`): systemtime(u64), localtime(u64), bias(i32),
  langid1/2/3, langstr, countrycode, countryabbrev, countryname — matches
  `t_client_countryinfo1` (bnet_protocol.h:565-578). ✓
- **PROGIDENT (0x06ff)** `decode_progident` (`codec_legacy_ols.cpp:33`):
  archtag, clienttag, versionid, unknown1 — matches `t_client_progident`
  (bnet_protocol.h:701-708). ✓
- **PROGIDENT2 (0x0bff)**: single `clienttag` field (bnet_protocol.h:2298). ✓

---

## FINDINGS

### F1 — `allowed_clients` gate dropped in the new auth FSM
- Severity: **MEDIUM**
- Classification: **BUG** (functional regression; UNSURE only on whether it is
  deliberately deferred — no TODO references it)
- Original ref: `src/bnetd/handle_bnet.cpp:544`
  ```cpp
  /* check if it's an allowed client type */
  if (tag_check_in_list(bn_int_get(packet->u.client_auth_info.clienttag), prefs_get_allowed_clients())) {
      conn_set_state(c, conn_state_destroy);
      return 0;
  }
  ```
  Same gate exists for PROGIDENT (`handle_bnet.cpp:643`) and PROGIDENT2
  (`handle_bnet.cpp:3602`).
- v3 ref: `src/protocol/bnet/src/fsm/fsm_auth.cpp:56` `BnetFsm::on(const AuthInfo&)`
  ```cpp
  if (auto tag = domain::ClientTag::from_packed_be(m.game_id)) {
      client_tag_ = tag.value();
  }
  state_ = BnetState::AuthInfoReceived;
  ... send AuthCheckReply{0u, ""}   // always "passed"
  ```
- Divergence: legacy rejects (destroys the connection) any client whose tag is
  not in the configured `allowed_clients` list. v3 accepts every printable
  4-byte tag unconditionally. The `allowed_clients` config IS still parsed
  (`infra/config/server_config.cpp`) but is never consulted on this path.
  Operators who restrict client types (e.g. WAR3/W3XP only) would silently
  have that restriction ignored.
- Proposed fix: in `on(AuthInfo)` (and the future ProgIdent/ProgIdent2
  handlers), consult the allowed-clients list (e.g. via `tag_check_in_list`
  or a domain port) and reject/destroy the session when the tag is absent,
  before sending the success `AuthCheckReply`.

### F2 — `from_packed_be` accepts any printable bytes; unknown tag silently kept as default
- Severity: **LOW**
- Classification: **UNSURE** (likely intentional permissiveness, but worth a note)
- v3 ref: `src/domain/shared/include/domain/shared/client_tag.hpp:59`
  `from_packed_be` only validates 0x20–0x7E printable ASCII; it does NOT check
  membership in the known-client set. In `fsm_auth.cpp:62`, if the result is an
  error (non-printable), the code falls through and **keeps the previous
  `client_tag_`** (default-constructed `"    "`), silently — there is no log
  and no rejection.
- Original ref: legacy stored the raw uint via `conn_set_clienttag` regardless
  of validity, but downstream code branches on `tag_check_client()` /
  `clienttag_uint_to_str()` returning UNKN. v3's `ClientTag::is_valid_client()`
  exists but is not called on this path.
- Divergence: an AUTH_INFO with a non-printable `game_id` leaves the session
  with a blank tag and no diagnostic; legacy would still have recorded the raw
  value. Minor, but could mask malformed clients.
- Proposed fix: on parse failure log + reject, or at least store `UNKN`
  explicitly; consider validating against `is_valid_client()` for the
  allowed-clients decision in F1.

### F3 — country / tzbias / archtag / gamelang / versionid not propagated from AUTH_INFO
- Severity: **LOW** (becomes MEDIUM once locale/timezone-dependent features land)
- Classification: **NOT-IMPLEMENTED** (rewrite in progress; FSM comment says
  "Phase-5 use-case will plug version-check policy here")
- Original ref: `src/bnetd/handle_bnet.cpp:565-570`
  ```cpp
  conn_set_country(c, langstr);   /* FIXME: ... we want USA not ENU */
  conn_set_tzbias(c, uint32_to_int(tzbias));
  conn_set_versionid(c, ...versionid);
  conn_set_archtag(c, ...archtag);
  conn_set_clienttag(c, ...clienttag);
  conn_set_gamelang(c, ...gamelang);
  ```
  COUNTRYINFO1 path likewise does `conn_set_country(c, country)` +
  `conn_set_tzbias` (`handle_bnet.cpp:510-511`).
- v3 ref: `fsm_auth.cpp:56` only stores `client_tag_`. The decoder DOES expose
  `archtag` (platform_id), `gamelang` (language_id), `versionid`, `tz_bias`,
  `country_abbr`, `country`, but none are persisted to a connection/account.
  `on(const CountryInfo1&)` (`fsm_auth.cpp:214`) is a no-op stub.
- Divergence: connection country, timezone bias, archtag, gamelang and
  versionid are currently lost. Anything depending on per-conn country/tzbias
  (channel routing, `bnettime` tz adjustment seen at legacy `handle_bnet.cpp:3082`
  `bnettime_add_tzbias(bn_time, -conn_get_tzbias(c))`) will be wrong once wired.
- Note for whoever implements it: replicate the legacy quirk
  `conn_set_country(c, langstr)` — country is taken from the **first** string
  (`country_abbr`/langstr), NOT the second (`country`/countryname). Easy to get
  backwards given the field names.
- Proposed fix: when the connection/account abstraction is wired, populate
  country (from `country_abbr`), tz_bias, archtag, gamelang, versionid from the
  decoded `AuthInfo`, and country+tzbias from `CountryInfo1`.

### F4 — AUTH_INFO `tz_bias` typed unsigned (no signed conversion)
- Severity: **LOW** (latent; not yet consumed)
- Classification: **UNSURE / NOT-IMPLEMENTED**
- Original ref: `handle_bnet.cpp:566` `conn_set_tzbias(c, uint32_to_int(tzbias))`
  — bias is signed `(gmt-local)/60`; legacy explicitly converts u32→int.
- v3 ref: `AuthInfo::tz_bias` is `std::uint32_t`
  (`messages/messages_auth.hpp:25`); `decode_auth_info` reads it as raw u32 with
  no sign reinterpretation. By contrast `CountryInfo1::bias` IS correctly
  `std::int32_t` (`codec_legacy_ols.cpp:95`). Inconsistent.
- Divergence: a western-hemisphere client (negative bias, e.g. 0xFFFFFFC4) would
  be interpreted as a huge positive offset if `tz_bias` is ever used in
  arithmetic without a cast. Harmless today (unused) but a trap for F3's
  implementer.
- Proposed fix: make `AuthInfo::tz_bias` a `std::int32_t` (matching
  CountryInfo1) and decode via signed reinterpret, OR cast at the consumption
  site, mirroring legacy `uint32_to_int`.

---

## Notes / non-issues
- SID_SYSTEMINFO / SID_LOCALEINFO: these are not distinct packets in this
  PvPGN lineage (the relevant init/locale carriers are AUTH_INFO,
  COUNTRYINFO1, PROGIDENT, PROGIDENT2). Nothing missing.
- `anongame_tags.cpp` (URL/MAP/TYPE/DESC/LADR payload codecs) is the
  FINDANONGAME tag system, unrelated to client identification; payload shapes
  look internally consistent. Out of scope for clienttag/locale.
