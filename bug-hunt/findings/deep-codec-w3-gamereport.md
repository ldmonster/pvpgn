# Deep codec pass — large variable-length BNCS bodies (W3 + game-report)

Scope: exhaustive field-by-field byte diff of the *variable-length* bodies an
earlier shallow `bnet-codec.md` pass skipped.

- ORIGINAL: `/home/cnupt/work/pvpgn-server`
- CURRENT v3: `/home/cnupt/work/pvpgn`

Subsystem bodies examined:
- `SID_GAMELISTREQ`/`SID_GAMELISTREPLY` (0x09) variable game-record list
- `CLIENT_GAME_REPORT` (0x2c) — game-report results + stats blob
- `SID_STATSREQ`/`SID_STATSREPLY` (0x26) — the W3 profile/stats reply
  (= the v3 `READUSERDATA` read req/reply)
- `SID_WRITEUSERDATA` (0x27) write request
- W3 NLS/SRP family: `CREATEACCOUNT_W3` (0x52), `LOGINREQ_W3` (0x53),
  `LOGONPROOFREQ` (0x54), `PASSCHANGEREQ` (0x55), `PASSCHANGEPROOFREQ` (0x56)
- W3 clan member info: `SID_CLANMEMBERLIST` reply (0x7d), `CLANMEMBERUPDATE` (0x7f)
- `STARTGAME1/3/4` variable bodies (0x08 / 0x1a / 0x1c)

> NOTE: There is **no `SID_STARTADVEX3`** symbol in either tree. In this PvPGN
> lineage the advertise/list packet is `GAMELISTREQ`/`GAMELISTREPLY` (0x09); that
> is the body audited here. `SID_REPORTVERSION` likewise does not exist as a
> distinct packet in this tree (version reporting rides `AUTHREQ`/`AUTHCHECK`,
> out of this subsystem).

---

## Summary

**Real wire-format bugs found: 0.**

Every variable-length body in this subsystem matches the original field order,
widths, endianness, NUL-termination, count fields, and per-element block layout.
Two items below are recorded as UNSURE / observation — neither is a wire-byte
divergence on the path PvPGN actually emits; they are decode-side robustness /
use-case-layer responsibilities.

---

## Findings

### F-1 — STATSREPLY value vector: decode requires exactly name×key cells, original may emit fewer (empty-key skip)

- Severity: LOW
- Classification: UNSURE (decode-side robustness; NOT a wire bug on the encode path)
- Original ref: `src/bnetd/handle_bnet.cpp:1528-1538` (`_client_statsreq`)
  ```c
  for (i=0, name_off=...; i<name_count && (name=...); i++, ...) {       // rows = names
      for (j=0, key_off=keys_off; j<key_count && (key=...); j++, ...) { // cols = keys
          if (*key == '\0') continue;                                   // <-- skips empty key
          packet_append_string(rpacket, _attribute_req(reqacc, myacc, key));
      }
  }
  ```
  Header `name_count`/`key_count` are echoed verbatim (`:1522-1524`), but the
  number of value strings actually appended is
  `name_count * (key_count - <#empty-keys>)`, i.e. **may be fewer than
  name_count×key_count**.
- v3 ref: `src/protocol/bnet/src/codec/codec_game.cpp:200-219`
  (`decode_userdata_read_reply`) computes `cells = name_count*key_count` and
  insists on reading **exactly** that many cstrings.
- Divergence: none on the bytes PvPGN-v3 *writes* — the v3 encoder
  (`codec_game.cpp:337-344`) writes `m.values.size()` strings and echoes the
  header counts, exactly mirroring the original. The mismatch is only that the
  v3 *decoder* would reject an authentic Battle.net reply that contained an
  empty key (value count < name×key). Battle.net/PvPGN servers don't normally
  send empty keys in a stats reply, so this is latent.
- Proposed fix (defensive only): in `decode_userdata_read_reply`, read values
  until the reader is empty (or `<= cells`) rather than requiring exactly
  `cells`, matching the original "append until keys/names exhausted, skipping
  empties" semantics. Not required for PvPGN-emitted traffic.

### F-2 — GAMELISTREPLY gamecount is derived from entries.size(); original error path sends sstatus≠0 with gamecount=0 and no records

- Severity: INFO
- Classification: INTENTIONAL (representable; not a codec divergence)
- Original ref: `src/bnetd/handle_bnet.cpp:3863-3925` (`_client_gamelistreq`):
  for a *specific* game request that fails (STARTED/FULL/PASS) the server sets
  `sstatus` to the error code, `gamecount = 0`, and appends **no** game record;
  for the LOADED case it sets `sstatus = 0x0a00`, appends one record, and
  `gamecount = 1`.
- v3 ref: `codec_game.cpp:346-377` (`encode(GameListReply)`) writes
  `entries.size()` as gamecount and `m.sstatus` independently.
- Divergence: none. The v3 `GameListReply{ sstatus, entries }` struct can
  represent every original case: error → `sstatus=err, entries={}` (gamecount
  encodes to 0); loaded → `sstatus=0x0a00, entries={one}` (gamecount=1). The
  spacer-before-every-record-except-first rule (`if(!first)`,
  `codec_game.cpp:355-361`) exactly mirrors the original
  `if (cbdata->counter)` gate (`handle_bnet.cpp:3813-3815`). Recorded only so a
  reviewer knows the gamecount-vs-sstatus coupling was checked.

---

## Bodies verified MATCH field-for-field (coverage)

### GAMELISTREQ (CLIENT, 0x09) — `decode_game_list_req` / `encode(GameListRequest)`
Original `t_client_gamelistreq` (`bnet_protocol.h:2638-2648`):
`gametype:u16, unknown1:u16, unknown2:u32, unknown3:u32, maxgames:u32, <gamename cstr>`.
v3 (`codec_game.cpp:13-27,335-344`): identical order/widths/LE; trailing cstring. **MATCH.**

### GAMELISTREPLY (SERVER, 0x09) — header + per-game record
Original header `t_server_gamelistreply` (`:2752-2758`): `gamecount:u32, sstatus:u32`.
Original per-record `t_server_gamelistreply_game` (`:2767-2784`):
`gametype:u16(LE), unknown1:u16(LE), unknown3:u16(LE), port:u16(BE), game_ip:u32(BE),
unknown4:u32, unknown5:u32, status:u32, unknown6:u32, <name cstr><pass cstr><info cstr>`,
with a 4-byte spacer dword (value 1) before every record after the first.
v3 (`codec_game.cpp:29-76,346-377`): identical — `read_le`/`write_le` for the
three u16s, `read_be`/`write_be` for `port` (u16) and `game_ip` (u32), four LE
u32s, three cstrings, spacer via `if(!first)`. **MATCH (incl. BE port/IP).**

### CLIENT_GAME_REPORT (0x2c) — `decode_game_report` / `encode(GameReport)`
Original `t_client_game_report` (`:3518-3527`): `unknown1:u32, count:u32`, then
`count × t_client_game_report_result{result:u32}` (`:3529-3532`), then
`count × <player name cstr>`, then `<report head cstr><report body cstr>`
(`handle_bnet.cpp:4358-4429`). The report head/body are the comma/CR-delimited
stats strings; the codec transports them as opaque cstrings exactly as the
original `packet_get_str_const` does (no byte-level parsing in either codec).
v3 (`codec_game.cpp:210-235,474-487`): `unknown1:u32, count:u32, count×u32
result, count×cstring name, header cstr, body cstr`. **MATCH.** (v3 adds a
defensive `count <= remaining/4` bound; safe, no format change.)

### STATSREQ (CLIENT, 0x26) — `decode_userdata_read_request` / `encode(UserDataReadRequest)`
Original `t_client_statsreq` (`:2024-2032`): `name_count:u32, key_count:u32,
requestid:u32`, then `name_count × <name cstr>`, then `key_count × <key cstr>`.
v3 (`codec_w3.cpp:184-198,327-335`): identical order/widths; names then keys. **MATCH.**

### STATSREPLY (SERVER, 0x26) — `decode_userdata_read_reply` / `encode(UserDataReadReply)`
Original `t_server_statsreply` (`:2078-2086`): `name_count:u32, key_count:u32,
requestid:u32`, then value cstrings (row-major name×key, see F-1).
v3 (`codec_w3.cpp:200-219,337-344`): same header; values as flat cstring list. **MATCH** on
header/widths/order; see F-1 for the empty-key edge case.

### WRITEUSERDATA (CLIENT, 0x27) — `decode_userdata_write_request` / encode
v3 (`codec_w3.cpp:221-245,346-354`): `name_count:u32, key_count:u32`, then names,
keys, then name×key value cstrings (NO requestid — correct, the write request
has no requestid field, unlike the read request). **MATCH.**

### CREATEACCOUNT_W3 (0x52) req/reply
Original req `t_client_createaccount_w3` (`:1761-1768`): `salt[32]:bytes,
password_verifier[32]:bytes, <name cstr>`. Reply `t_server_createaccount_w3`
(`:1794-1799`): `result:u32`.
v3 (`codec_w3.cpp:9-33,252-264`; struct `messages_realm.hpp:189-205`): salt[32],
verifier[32], name; reply result:u32. **MATCH.**

### LOGINREQ_W3 / LOGINREPLY_W3 (0x53)
Original req `t_client_loginreq_w3` (`:1630-1635`): `client_public_key[32], <name cstr>`.
Reply `t_server_loginreply_w3` (`:1650-1657`): `message:u32, salt[32], server_public_key[32]`.
v3 (`codec_w3.cpp:37-67,266-279`; struct `messages_realm.hpp:215-227`). **MATCH.**

### LOGONPROOFREQ / LOGONPROOFREPLY (0x54)
Original req `t_client_logonproofreq` (`:1690-1694`): `client_password_proof[20]`.
Reply `t_server_logonproofreply` (`:1697-1702`): `response:u32, server_password_proof[20]`,
followed by an **optional trailing cstring** when `response == CUSTOM (0x0f)`
(account-locked text, appended at `handle_bnet.cpp:2195-2198`).
v3 (`codec_w3.cpp:71-93,281-295`; struct `messages_realm.hpp:237-249`) reads/writes
the optional trailing `message` cstring guarded by `!r.empty()` / `!m.message.empty()`.
**MATCH (incl. the optional custom-message tail).**

### PASSCHANGEREQ / PASSCHANGEREPLY (0x55)
Original req `t_client_passchangereq` (`:1711-1716`): `client_public_key[32], <name cstr>`.
Reply `t_server_passchangereply` (`:1719-1725`): `message:u32, salt[32], server_public_key[32]`.
v3 (`codec_w3.cpp:97-125,297-310`; struct `messages_realm.hpp:260-272`). **MATCH.**

### PASSCHANGEPROOFREQ / PASSCHANGEPROOFREPLY (0x56)
Original req `t_client_passchangeproofreq` (`:1732-1738`):
`client_password_proof[20], salt[32], password_verifier[32]`.
Reply `t_server_passchangeproofreply` (`:1741-1746`): `response:u32, server_password_proof[20]`.
v3 (`codec_w3.cpp:129-162,312-325`; struct `messages_realm.hpp:281-293`). **MATCH.**

### CLANMEMBERLIST_REQ / REPLY (0x7d)
Original req `t_client_clanmemberlist_req` (`:4048-4051`): `count:u32` (cookie).
Original reply header `t_server_clanmemberlist_reply` (`:4025-4040`):
`count:u32, member_count:u8`, then per member (built in
`clan.cpp:384-409`): `<name cstr>, status:u8, online_status:u8, <location cstr>`.
v3 (`codec_clan.cpp:255-293,486-503`; structs `messages_clan.hpp:180-189`):
cookie:u32, member_count:u8, per member name/rank:u8/online:u8/location cstr. **MATCH.**

### CLANMEMBERUPDATE (SERVER, 0x7f)
Original built in `clan.cpp:579-593` / `615-629`:
`<name cstr>, status:u8, online:u8, <location cstr>` (the 2-byte `tmpstr` block =
status byte + online byte, then the location string; the header comment's
"bn_short" is misleading — actual wire is two bytes + cstring).
v3 (`codec_clan.cpp:303-315,511-518`): name, rank:u8, online:u8, location cstr. **MATCH.**

### STARTGAME1 (0x08) / STARTGAME3 (0x1a) / STARTGAME4 (0x1c) request bodies
Original `t_client_startgame1` (`:2802-2814`): `status:u32, unknown3:u32,
gametype:u16, unknown1:u16, unknown4:u32, unknown5:u32, name/pass/info cstrs`.
Original `t_client_startgame3` (`:2855-2868`): `status:u32, unknown3:u32,
gametype:u16, unknown1:u16, unknown6:u32, unknown4:u32, unknown5:u32, name/pass/info`.
Original `t_client_startgame4` (`:2920-2933`): `status:u16, flag:u16, unknown2:u32,
gametype:u16, option:u16, unknown4:u32, unknown5:u32, name/pass/info`.
v3 (`codec_game.cpp:149-194,78-104,424-457,379-392`): all three match field order,
the u16/u32 widths, and the trailing three cstrings exactly. **MATCH** (note the
distinct STARTGAME4 leading two u16s `status`/`flag` vs STARTGAME1/3's leading
two u32s — correctly reproduced).

### JOIN_GAME (0x22) — `decode_join_game`
Original `t_client_join_game` (`:3548-3554`): `clienttag:u32, versiontag:u32,
<gamename cstr><password cstr>`. v3 (`codec_game.cpp:198-206,465-472`). **MATCH.**

---

## Out of scope / not implemented (noted, not bugs in this subsystem)

- `w3route_wire_types.hpp` is **constants-only**; the W3 anongame-routing
  packets (W3ROUTE gameresult with its variable player/hero blocks,
  `bnet_protocol.h:73-396`) have **no codec implemented yet**
  (NOT-IMPLEMENTED). They belong to the w3route/anongame subsystem, not the
  BNCS codec audited here.
- Realm-list and ladder variable bodies in `codec_realm.cpp` / `codec_ladder.cpp`
  are covered by the separate `realm-serverlist-udp.md` / `ladder.md` passes.
