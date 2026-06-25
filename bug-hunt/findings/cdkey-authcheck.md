# Bug-hunt: CD-key handling / SID_AUTH_CHECK / SID_AUTH_INFO / version-check / spawn

Subsystem owner: CD-key + AUTH_CHECK + AUTH_INFO + version/checkrevision + spawn.

ORIGINAL: `/home/cnupt/work/pvpgn-server`
CURRENT v3: `/home/cnupt/work/pvpgn`

## TL;DR

- **The CD-key WIRE LAYOUT and all result-code CONSTANTS in v3 MATCH the original byte-for-byte.** No layout or constant divergences found in `cdkey_wire_types.hpp`, `auth_wire_types.hpp`, the `decode_*` / `encode_*` codecs, or the SID code map.
- **The original PvPGN never validates or hashes CD-keys** — it stores the owner/cdkey strings and always replies OK. So v3 "not validating cdkeys" is *faithful behaviour*, not a bug.
- **The real bug is in the FSM handshake sequencing** (`fsm_auth.cpp`): the v3 `on(AuthInfo)` handler replies with the *wrong packet* (`AuthCheckReply` = SID_AUTH_CHECK 0x51) instead of the version-check seed (`AuthInfoReply` = SERVER_AUTHREQ_109 0x50), and `on(AuthCheckRequest)` sends *no reply at all*. Version-check is entirely unimplemented (always-passes), which is fine as a stub, but the packet that *is* sent is the wrong one for the wrong protocol step.

---

## FINDING 1 — AUTH_INFO handler emits the wrong reply packet (SID_AUTH_CHECK instead of the version-check seed)

- **Severity:** HIGH (protocol-breaking for any real Battle.net client; a real D2/SC/W3 client will not proceed)
- **Classification:** BUG

**Original** — `src/bnetd/handle_bnet.cpp:582-620` (`_client_auth_info`, handling SID_AUTH_INFO 0x50):
```cpp
// reply to SID_AUTH_INFO is SERVER_AUTHREQ_109 (0x50): the version-check SEED
packet_set_size(rpacket, sizeof(t_server_authreq_109));
packet_set_type(rpacket, SERVER_AUTHREQ_109);          // 0x50ff
bn_int_set(&...logontype, ...);
bn_int_set(&...sessionkey, conn_get_sessionkey(c));
bn_int_set(&...sessionnum, conn_get_sessionnum(c));
... select_checkrevision(...) ...
file_to_mod_time(c, ..., &...timestamp);
packet_append_string(rpacket, checkrevision_filename);  // mpq filename
packet_append_string(rpacket, checkrevision_equation);  // version-check equation
// + 128-byte zero pad for W3/W3XP
```
The OK/badversion *result* (`SERVER_AUTHREPLY_109` 0x51) is sent **later**, from the AUTH_CHECK handler `_client_authreq109` (`handle_bnet.cpp:1250`).

**v3** — `src/protocol/bnet/src/fsm/fsm_auth.cpp:56-70`:
```cpp
core::Status<> BnetFsm::on(const AuthInfo& m) {
    ...
    state_ = BnetState::AuthInfoReceived;
    // ack with result=0 ("passed") + empty info ...
    return ctx_->send(ServerMessage{AuthCheckReply{0u, ""}});   // <-- encodes as SID_AUTH_CHECK (0x51)
}
```
`encode(AuthCheckReply)` (`codec_auth.cpp:225`) calls `w.begin_bnet_packet(kSidAuthCheck)` = **0x51**.

**Divergence:** On SID_AUTH_INFO (0x50) v3 sends SID_AUTH_CHECK (0x51, result-only). It should send the SERVER_AUTHREQ_109 (0x50) *seed* — logontype, server_token/sessionkey, session_num, FILETIME timestamp, mpq filename, version-check equation, optional 128-byte W3 pad. The matching struct (`AuthInfoReply` + its `encode` at `codec_auth.cpp:232`, packet 0x50) already exists and is correctly laid out — it just isn't the one being sent. The result is the client never receives the checkrevision equation it must answer, and receives an unexpected 0x51 before it ever sent its 0x51.

**Proposed fix:** In `on(AuthInfo)`, send an `AuthInfoReply` (0x50) populated with logontype (0, or 2 for WAR3/W3XP), a server_token, session_num, timestamp, mpq_filename, checksum_formula (and the 128-byte zero `server_signature` for W3/W3XP). Move the result reply (`AuthCheckReply` 0x51) into `on(AuthCheckRequest)` — see Finding 2.

---

## FINDING 2 — AUTH_CHECK handler sends no reply (client hangs waiting for SERVER_AUTHREPLY_109)

- **Severity:** HIGH (client stalls at version-check; cannot reach login)
- **Classification:** BUG / NOT-IMPLEMENTED (stub returns ok() with no packet)

**Original** — `src/bnetd/handle_bnet.cpp:1139-1259` (`_client_authreq109`, SID_AUTH_CHECK 0x51). After reading exeinfo/owner/gameversion/checksum and running `select_versioncheck`, it **always** sends a `SERVER_AUTHREPLY_109` (0x51): `MESSAGE_OK` (0x0), `MESSAGE_UPDATE` (0x100) when an autoupdate mpq exists, or `MESSAGE_BADVERSION` (0x101) on version-check failure (`handle_bnet.cpp:1250` OK path, `:1156` badversion path).

**v3** — `src/protocol/bnet/src/fsm/fsm_auth.cpp:164-170`:
```cpp
core::Status<> BnetFsm::on(const AuthCheckRequest&) {
    if (state_ != BnetState::AuthInfoReceived && state_ != BnetState::Init) {
        return reject("bnet fsm: AUTH_CHECK out of order");
    }
    return core::ok();           // <-- no packet sent
}
```

**Divergence:** The AUTH_CHECK handler decodes nothing from the request and sends no reply. (Decoder `decode_auth_check_request` exists and is correct, but the handler ignores its output.) Combined with Finding 1, the OK reply is sent at the wrong step and the correct step sends nothing.

**Proposed fix:** Send `AuthCheckReply{result, info}` here. Minimal always-pass stub: `AuthCheckReply{auth::kServerAuthReply109MessageOk /*0*/, ""}`. (`kServerAuthReply109MessageOk` already defined in `auth_wire_types.hpp:62`.) When a real version-check use-case is wired, return `kServerAuthReply109MessageBadVersion` (0x101) / `...MessageUpdate` (0x100) as appropriate.

---

## FINDING 3 — Version-check / checkrevision is entirely unimplemented

- **Severity:** MEDIUM (security/compat: every client version unconditionally accepted; no autoupdate)
- **Classification:** NOT-IMPLEMENTED (consistent with prior finding "version-check always passes")

**Original:** `select_versioncheck(...)` + `select_checkrevision(...)` + `autoupdate_check(...)` + `prefs_get_allow_unknown_version()` (`handle_bnet.cpp:602`, `:1080`, `:1208`). Failing the check with `allow_unknown_version=false` sends BADVERSION and sets `conn_state_untrusted`.

**v3:** No equivalent anywhere under `src/protocol/bnet/src` or `src/application` (grep for `checkrevision|versioncheck|select_versioncheck|allow_unknown_version` finds only the wire fields `gameversion`/`spawn` being read, never evaluated). The `AuthInfo` handler comment says "Phase-5 use-case will plug version-check policy here."

**Proposed fix:** Track as a known gap; wire a version-check/checkrevision use-case (equation selection, file-timestamp for the mpq, autoupdate, allow-unknown-version preference) feeding Findings 1 & 2.

---

## FINDING 4 — Spawn flag is decoded but never acted on

- **Severity:** LOW
- **Classification:** NOT-IMPLEMENTED (matches original: original also does not branch on spawn in AUTH_CHECK)

**Original:** `t_client_authreq_109.spawn` exists (`bnet_protocol.h:915`) but `_client_authreq109` never reads `.spawn`. Spawn handling in the original lives only in the CDKEY2/CDKEY3 paths conceptually; the AUTH_CHECK handler ignores it. So v3 ignoring `m.spawn` is faithful.

**v3:** `decode_auth_check_request` reads `m.spawn` (`codec_auth.cpp:96`); the handler ignores it. **No divergence** vs original. Noted for completeness; no fix required unless spawn-copy gating is desired.

---

## VERIFIED MATCHES (no bug)

### AUTH_CHECK request field layout — MATCH
Original `t_client_authreq_109` (`bnet_protocol.h:907-919`): `ticks, gameversion, checksum, cdkey_number, spawn`, then N × cdkey_info, then exeinfo string, then owner string.
v3 `decode_auth_check_request` / `encode(AuthCheckRequest)` (`codec_auth.cpp:88-114`, `:268-290`): reads `ticks, gameversion, checksum, <count>, spawn`, then `count` × CdKeyInfo, then `exe_info`, then `cdkey_owner`. **Order and field count match.** (v3 stores count in the vector size rather than a struct field — semantically equivalent; bounds-checks count ≤ 8.)

### Per-CD-key block (`t_cdkey_info`) — MATCH
Original (`bnet_protocol.h:922-929`): `len, type, checksum, u1, hash[5]` (8 × u32 = 32 bytes).
v3 `CdKeyInfo` (`messages_auth.hpp:64-71`) + codec (`codec_auth.cpp:104-110`, `:280-286`): `public_value (legacy "len"), product (==type), checksum, unknown (==u1), hash[5]`. **Field-for-field match in order and size.**

### SERVER_AUTHREPLY_109 result codes — MATCH
Original (`bnet_protocol.h:889-891`): OK `0x00000000`, UPDATE `0x00000100`, BADVERSION `0x00000101`.
v3 `auth_wire_types.hpp:62-64`: `kServerAuthReply109MessageOk 0x0`, `...Update 0x100`, `...BadVersion 0x101`. **Match.**

### SERVER_AUTHREPLY1 (OLS 0x07) result codes — MATCH
Original (`bnet_protocol.h:871-873`): BADVERSION `0x0`, UPDATE `0x1`, OK `0x2`.
v3 `auth_wire_types.hpp:57-59`: same. **Match.**

### SERVER_AUTHREQ_109 (0x50, the AUTH_INFO seed) layout — MATCH
Original `t_server_authreq_109` (`bnet_protocol.h:783-792`): `logontype, sessionkey, sessionnum, timestamp(bn_long, LE via bn_long_set), mpqfilename str, equation str` (+128-byte W3 pad appended by handler).
v3 `AuthInfoReply` (`messages_auth.hpp:36-50`) + `encode(AuthInfoReply)` (`codec_auth.cpp:232-247`): `logontype, server_token, session_num, timestamp(u32 lo then u32 hi = LE), mpq_filename, checksum_formula, opaque server_signature`. **Match.** (Bug is only that this correct struct is *not the one sent* on AUTH_INFO — see Finding 1.)
Logontype constants also match: original `_W3`/`_W3XP` = `0x2` (`bnet_protocol.h:794-795`) vs v3 `kServerAuthReq109LogontypeW3`/`W3xp` = `0x2` (`auth_wire_types.hpp:53-54`).

### CDKEY2 (0x36) request layout + reply codes — MATCH
Original `t_client_cdkey2` (`bnet_protocol.h:1112-1124`): `spawn, keylen, productid, keyvalue1, sessionkey, ticks, key_hash[5]`, owner str.
v3 `CdKey2Request` / codec (`messages_auth.hpp:109-119`, `codec_auth.cpp:116-128`, `:292-303`): `spawn, keylen, product_id, key_value, server_token, ticks, key_hash[5]`, owner. **Match.**
Reply codes — original `SERVER_CDKEYREPLY2_MESSAGE_*` (`bnet_protocol.h:1886-1890`): OK 1, BAD 2, WRONGAPP 3, ERROR 4, INUSE 5. v3 `cdkey_wire_types.hpp:24-28` (shared CdkeyReply set): same. **Match.**

### CDKEY3 (0x42) request layout + reply code + magic constants — MATCH
Original `t_client_cdkey3` (`bnet_protocol.h:1151-1164`): `unknown1..unknown7, key_hash[5]`, owner.
v3 `CdKey3Request` / codec (`codec_auth.cpp:141-158`, `:312-324`): `unknown1..unknown7, key_hash[5]`, owner_name. **Match.**
Reply OK code: original `SERVER_CDKEYREPLY3_MESSAGE_OK 0x0` (`bnet_protocol.h:1189`) vs v3 `kCdkeyReply3MessageOk 0x0` (`cdkey_wire_types.hpp:31`). **Match.**
Magic constants original `CLIENT_CDKEY3_UNKNOWN1..7` (`bnet_protocol.h:1165-1171`: `0xffffffff, 0x1, 0x0, 0x10, 0x6, 0x00123456, 0x0`) vs v3 `kCdkey3Unknown1..7` (`cdkey_wire_types.hpp:38-44`): same values in order. **Match.**

### CDKEY (0x30) reply codes + spawn flags — MATCH
Original `SERVER_CDKEYREPLY_MESSAGE_*` (`bnet_protocol.h:1081-1085`): OK 1..INUSE 5; `CLIENT_CDKEY2_SPAWN_TRUE/FALSE` = 1/0 (`bnet_protocol.h:1125-1126`).
v3 `cdkey_wire_types.hpp:24-35`: same. Packet type codes 0x30ff/0x36ff/0x42ff also match (`cdkey_wire_types.hpp:14-21`). **Match.**

### CD-key hashing algorithm — N/A (neither side implements it)
The original server performs **no** SHA-1/token-mixing validation of CD-keys in any AUTH_CHECK / CDKEY / CDKEY2 / CDKEY3 handler — it trusts the client and always replies OK (`handle_bnet.cpp:1304-1410`). No `cd_check` / `keytable` / `keydecode` / `bnet_decode_cdkey` code exists in the original tree (grep returned nothing). v3 likewise does no cdkey hashing. **No divergence to audit** — there is no client/server-token mixing or key-data SHA-1 to compare on either side.

---

## Summary of CONSTANTS / LAYOUTS that MATCH
AUTH_CHECK request layout; per-cdkey block (len/type/checksum/u1/hash[5]); SERVER_AUTHREPLY_109 codes (0/0x100/0x101); SERVER_AUTHREPLY1 codes (0/1/2); SERVER_AUTHREQ_109 seed layout + logontype (0x2); CDKEY/CDKEY2/CDKEY3 request layouts; all CDKEYREPLY/REPLY2/REPLY3 message codes; CDKEY2 spawn flags; CDKEY3 magic constants; all SID/packet type codes (0x50/0x51/0x36/0x42/0x30). Timestamp byte order (LE) matches.

## Bugs to fix
1. (HIGH) `on(AuthInfo)` must send `AuthInfoReply` (0x50 seed), not `AuthCheckReply` (0x51).
2. (HIGH) `on(AuthCheckRequest)` must send `AuthCheckReply` (0x51 result), currently sends nothing.
3. (MED) version-check/checkrevision unimplemented (always passes) — known gap.
4. (LOW) spawn flag decoded but unused — faithful to original, no action needed.
