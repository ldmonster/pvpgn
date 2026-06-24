# Bug Hunt: Realm list / Server list / UDP-NAT detection

Subsystem: D2 realm list (SID_REALMLISTREQ 0x34 / 0x40), server list (SID_SERVERLIST 0x04),
realm join (SID_REALMJOINREQ 0x3E), and UDP test / NAT "plug" detection
(SID_UDPPINGRESPONSE/CLIENT_UDPOK 0x14, the UDP datagram protocol, `udptest_send`).

ORIGINAL: `/home/cnupt/work/pvpgn-server`
CURRENT v3: `/home/cnupt/work/pvpgn`

Scope reminder: this report separates **NOT-IMPLEMENTED** (feature simply absent in v3)
from **implemented-but-wrong** (codec/constant divergence). The wire constants and codec
byte-layout in v3 are largely *correct*; the bugs are in **missing behavior**: the realm
handlers are stubs and the NAT-detection state machine does not exist.

---

## Summary table

| # | Severity | Class | Title |
|---|----------|-------|-------|
| 1 | HIGH | NOT-IMPLEMENTED | Realm-list request handlers are stubs — server never replies with realms |
| 2 | HIGH | NOT-IMPLEMENTED | NAT / UDP "plug" detection entirely absent (UdpOk is a no-op, kMfPlug is dead) |
| 3 | HIGH | NOT-IMPLEMENTED | No `udptest_send` equivalent — server never sends SERVER_UDPTEST to clients |
| 4 | MED  | NOT-IMPLEMENTED | RealmJoin request handler is a stub — D2 realm character-server handshake never replies |
| 5 | LOW  | BUG (doc) | Wrong SID values in fsm_chat.cpp doc comments (legacy 0x1C, join 0x41) |
| 6 | INFO | MATCH | UDP datagram codec, realm-reply wire layout, and all magic constants match |

---

## Finding 1 — Realm-list handlers are stubs; no reply is ever sent  [HIGH / NOT-IMPLEMENTED]

**Original** `src/bnetd/handle_bnet.cpp:2953` (`_client_realmlistreq`, 0x34) and
`:2996` (`_client_realmlistreq110`, 0x40): both walk `realmlist()`, skip inactive realms
(`!realm_get_active(realm)`), append per-realm fixed dwords + name + description, set the
`count` field, and push the reply onto the out-queue.

```c
LIST_TRAVERSE_CONST(realmlist(), curr) {
    realm = (t_realm*)elem_get_data(curr);
    if (!realm_get_active(realm)) continue;
    ... bn_int_set(realmdata fields) ...
    packet_append_data(rpacket, &realmdata, sizeof(realmdata));
    packet_append_string(rpacket, realm_get_name(realm));
    packet_append_string(rpacket, realm_get_description(realm));
    count++;
}
bn_int_set(&rpacket->u.server_realmlistreply.count, count);
conn_push_outqueue(c, rpacket);
```

**v3** `src/protocol/bnet/src/fsm/fsm_chat.cpp:465` and `:477`:

```cpp
core::Status<> BnetFsm::on(const RealmListRequest&) {
    return require_clan_state(state_, "bnet fsm: REALMLISTREQ before login");
}
core::Status<> BnetFsm::on(const RealmListLegacyRequest&) {
    return require_clan_state(state_, "bnet fsm: REALMLISTREQ (legacy) before login");
}
```

The handlers only validate that the client is logged-in and return `ok()`. They never:
- call `application::realm::dispatch_realm_list(...)`,
- build a `RealmListReply` / `RealmListLegacyReply`,
- send anything back to the client.

**Divergence:** A Diablo II client that sends SID_REALMLISTREQ (to pick a realm before
character selection) receives **no reply**. The realm selection screen will hang / show an
empty list, blocking D2 closed-realm play entirely.

Note the supporting machinery already exists but is **dangling / unused**:
- `application::realm::dispatch_realm_list` is defined in
  `src/application/realm/src/realm_list.cpp:6` but has **zero callers** anywhere in the tree.
- The codec encoders `encode(Writer&, const RealmListReply&)` /
  `encode(..., const RealmListLegacyReply&)` exist in
  `src/protocol/bnet/src/codec/codec_realm.cpp:263` / `:347` and are correct (see Finding 6),
  but are never invoked from a handler.

**Proposed fix:** In `on(RealmListRequest)` / `on(RealmListLegacyRequest)`, after the
login-state check: obtain the realm repository, call `dispatch_realm_list`, map each active
`RealmListing` into a `RealmListEntry` (0x40: `unknown=1`, name, description) or
`RealmListLegacyEntry` (0x34: the 7 fixed dwords, name, description), and
`ctx_->send(ServerMessage{RealmListReply{...}})`. The default field values in the message
structs already carry the correct magic constants.

---

## Finding 2 — NAT / UDP "plug" detection entirely absent  [HIGH / NOT-IMPLEMENTED]

This is the core of the subsystem and is missing end-to-end.

**Original mechanism:**
1. On bnet-class connect, every connection starts plugged:
   `src/bnetd/connection.cpp:383` → `temp->protocol.flags = MF_PLUG;`
   (`MF_PLUG = 0x00000010`, "tiny plug to right of icon, no UDP",
   `src/common/bnet_protocol.h:2578`).
2. Server sends a UDP test packet (`udptest_send`, Finding 3) when the conn becomes bnet-class
   (`connection.cpp:793-795`).
3. Client echoes the bnettag back over TCP as CLIENT_UDPOK (0x14ff), echo == `"tenb"`
   (`bnet_protocol.h:1899`, `t_client_udpok.echo`).
4. `_client_udpok` (`handle_bnet.cpp:1417`) calls `conn_set_udpok(c)`
   (`connection.cpp:3244`), which sets `conn_flags_udpok` and **clears MF_PLUG**:
   ```c
   c->protocol.cflags |= conn_flags_udpok;
   c->protocol.flags  &= ~MF_PLUG;
   ```
5. `MF_PLUG` then propagates into the user's flags shown in chat/userinfo — and, critically,
   a plugged client's games are flagged as non-UDP-reachable (the "behind NAT, not joinable"
   indication).

**v3:**
- `kMfPlug = 0x00000010` is declared in
  `src/protocol/bnet/include/protocol/bnet/chat_wire_types.hpp:70` but is a **dead constant**
  — `grep` for `kMfPlug` across all `.cpp` returns **no usages**. Nothing ever sets it on
  connect; nothing ever clears it.
- `on(const UdpOk&)` is an explicit no-op:
  `src/protocol/bnet/src/fsm/fsm_misc.cpp:42`
  ```cpp
  core::Status<> BnetFsm::on(const UdpOk&) {
      // UDP echo confirmation may arrive in any state ... advisory ...
      return core::ok();
  }
  ```
  It does not validate the echoed token, does not set a "udp ok" flag, and does not clear a
  plug flag (there is no plug flag to clear).

**Divergence:** v3 never tracks whether a client's UDP port is reachable. The MF_PLUG icon /
NAT indicator and any downstream "is this game joinable over UDP" logic is gone. Functionally,
in the original a client that fails the UDP test stays plugged; in v3 the concept does not
exist at all (clients are never plugged, never tested, never confirmed).

**Proposed fix:** Reintroduce a per-connection `udp_ok` flag (default false ⇒ plugged).
On `on(UdpOk)`, optionally validate `m.echo == kBnetTag`/`"tenb"`, then mark the connection
`udp_ok` and clear the plug bit. Feed the plug bit into the chat/userinfo flags composition.
Requires Finding 3 (sending the test) to be meaningful.

---

## Finding 3 — No `udptest_send` equivalent; SERVER_UDPTEST is never sent  [HIGH / NOT-IMPLEMENTED]

**Original** `src/bnetd/udptest_send.cpp:43` `udptest_send()`: builds a `t_server_udptest`
(type `SERVER_UDPTEST = 0x00000005`, `bnettag = "bnet"`), sends it via `psock_sendto` to the
client's game addr/port, retrying up to 5 times until 2 successes. Invoked from
`connection.cpp:795` (on bnet-class) and from `handle_udp.cpp:92,124` (on SESSIONADDR1/2).

**v3:** The UDP *codec* can serialize this datagram —
`src/protocol/udp/src/codec.cpp:48` (`UdpTest` visitor emits `kServerUdpTest` + `bnettag`) —
and `infra::net::UdpEndpoint` (`src/infra/net/src/udp_endpoint.cpp`) is a generic
send/receive transport. But **nothing wires them together**:
- `grep` for `UdpTest`, `kServerUdpTest`, `SessionAddr`, `udp::encode`, `udp::decode`,
  `Datagram` across all `.cpp` matches **only** `src/protocol/udp/src/codec.cpp` itself.
- No code constructs a `udp::Datagram{UdpTest{...}}`, no code calls `udp::encode`, and no
  code calls `UdpEndpoint::send_to` with a UDP-test payload.
- There is no consumer for `UdpEndpoint::on_datagram` that decodes inbound CLIENT_UDPPING /
  CLIENT_SESSIONADDR1 / CLIENT_SESSIONADDR2 (no analogue of `handle_udp_packet`).

**Divergence:** The server never probes the client's UDP port, so the UDP-OK handshake
(Finding 2) can never legitimately complete, and SESSIONADDR-based game-address learning is
absent. The `udp::decode`/`encode` and `UdpEndpoint` are present but dead with respect to the
BNCS UDP test flow.

**Proposed fix:** Add a UDP service that (a) on bnet-class/handshake sends
`UdpTest{kBnetTag}` to the client's game endpoint, and (b) on inbound datagrams decodes via
`udp::decode` and handles UDPPING (debug), SESSIONADDR1 (find conn by session key, set game
addr/port, resend UDP test) and SESSIONADDR2 (find by session number, verify session key) —
mirroring `handle_udp.cpp`.

---

## Finding 4 — RealmJoin handler is a stub  [MED / NOT-IMPLEMENTED]

**Original** SID_REALMJOINREQ_109 (0x3eff) is handled to return the D2 character-server
(D2CS/D2DBS) join reply (`SERVER_REALMJOINREPLY_109`, `bnet_protocol.h:3595`), carrying the
realm server address/port, session key, and account name.

**v3** `src/protocol/bnet/src/fsm/fsm_chat.cpp:469`:
```cpp
core::Status<> BnetFsm::on(const RealmJoinRequest&) {
    return require_clan_state(state_, "bnet fsm: REALMJOINREQ before login");
}
```
Login-state check only; never builds/sends a `RealmJoinReply`. The codec encoder
`encode(Writer&, const RealmJoinReply&)` exists and is correct
(`codec_realm.cpp:287`, including the lone big-endian `port` field), but is never invoked.

**Divergence:** After a client selects a realm, the realm-join handshake produces no reply,
so the client cannot be handed off to the D2 realm server. (Lower severity only because it is
downstream of Finding 1, which already blocks realm selection.)

**Proposed fix:** Implement realm-join: look up the realm, allocate/derive the session, fill
`RealmJoinReply` (realm addr/port big-endian, session key/num, account name) and send it.

---

## Finding 5 — Wrong SID values in fsm_chat.cpp doc comments  [LOW / BUG (documentation)]

**v3** `src/protocol/bnet/src/fsm/fsm_chat.cpp`:
- Line 22: `on(RealmListRequest) — SID_REALMLISTREQ (0x40)` — correct.
- Line 25: `on(RealmListLegacyRequest)— SID_REALMLISTREQ legacy (0x1C)` — **wrong**; the
  legacy realm list is **0x34** (`CLIENT_REALMLISTREQ 0x34ff`, `bnet_protocol.h:1197`), and
  the actual constant `kSidRealmListLegacy = 0x34` is correct in
  `messages_common.hpp:67`. 0x1C is unrelated.
- Line 23: `on(RealmJoinRequest) — SID_REALMJOINREQ (0x41)` — **wrong**; realm join is
  **0x3E** (`kSidRealmJoin = 0x3E`, `messages_common.hpp:63`;
  `CLIENT_REALMJOINREQ_109 0x3eff`, `bnet_protocol.h:3584`). 0x41 is unrelated.

Comments only — no runtime effect, but misleading. **Fix:** correct the comments to 0x34 and
0x3E.

---

## Finding 6 — What MATCHES (verified correct)  [INFO / MATCH]

These were checked byte-for-byte and are faithful to the original:

**UDP datagram protocol** (`src/protocol/udp/include/protocol/udp/wire_types.hpp`,
`src/protocol/udp/src/codec.cpp` vs `src/common/udp_protocol.h`):
- `kServerUdpTest = 0x05`, `kClientUdpPing = 0x07`, `kClientSessionAddr1 = 0x08`,
  `kClientSessionAddr2 = 0x09` — all match `SERVER_UDPTEST` / `CLIENT_UDPPING` /
  `CLIENT_SESSIONADDR1` / `CLIENT_SESSIONADDR2`.
- `kBnetTag = 0x626E6574` ('b','n','e','t') matches `BNETTAG "bnet"` (`common/tag.h:181`);
  written LE it is the `74 65 6E 62` ("tenb") seen on the wire — consistent with original.
- Codec field order: type then payload; SessionAddr2 = key then num. Correct LE encoding,
  all reads bounds-checked.

**Realm-list reply wire layout** (`codec_realm.cpp` + `messages_realm.hpp` +
`realm_wire_types.hpp` vs `handle_bnet.cpp` + `bnet_protocol.h:1197-1307`):
- 0x40 (110) reply: `unknown1` (u32) + `count` (u32) + per-entry { `unknown`(=1 default),
  name cstring, description cstring }. Matches `t_server_realmlistreply_110` exactly
  (`SERVER_REALMLISTREPLY_110_DATA_UNKNOWN1 = 0x00000001`).
- 0x34 (legacy) reply: `unknown1` + `count` + per-entry { 7 fixed dwords, name, description }.
  The 7 defaults in `RealmListLegacyEntry` —
  `0xC0000000, 0, 0, 0, 0x00018210, 0xFFFFFFFF, 0` — exactly match
  `SERVER_REALMLISTREPLY_DATA_UNKNOWN3..9` (`bnet_protocol.h:1272-1278`). Field **order**
  matches the original (`unknown3..9` then name then description).
- Both encoders enforce a 256-entry cap (`kRealmListLimit`); original had no cap but this is
  a defensive improvement, not a divergence.

**RealmJoinReply layout** (`codec_realm.cpp:287`): the 14 fixed u32s, the lone big-endian
`port` (`write_be<u16>`), the LE `u3`, the 5-word `secret_hash`, and trailing account-name
cstring match `t_server_realmjoinreply_109`.

**SID type codes** (`messages_common.hpp`): `kSidServerList=0x04`, `kSidRealmList=0x40`,
`kSidRealmJoin=0x3E`, `kSidRealmListLegacy=0x34`, `kSidUdpOk=0x14` — all correct.

**ServerList (0x04) encoder** (`codec_realm.cpp:243`): `unknown1`(u32) + `servers` cstring
matches `t_server_serverlist` (`bnet_protocol.h:2345`).

---

## Net assessment

The wire formats are right; the **behavior is missing**. The three high-severity items
(realm-list reply, NAT/plug detection, UDP test send) are all NOT-IMPLEMENTED: the encoders,
the `dispatch_realm_list` application function, the `udp::codec`, `UdpEndpoint`, `kMfPlug`,
and the `UdpOk` FSM hook all exist as scaffolding but are never wired into a working flow.
For D2 closed-realm support and the BNCS "behind-NAT" indicator to function, these handlers
must be filled in. No implemented-but-wrong codec/constant bugs were found beyond the two
incorrect doc-comment SID values in Finding 5.
