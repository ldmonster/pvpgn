# Step 4 — Packet/Queue migration checklist

Tracks progress against [step4-packet-queue-audit.md](step4-packet-queue-audit.md).
Update after every landed slice. All boxes are ASCII (` `, `x`, `~`).

Legend: `[ ]` = not started, `[~]` = in progress, `[x]` = done.

## E.1  v3 surface additions

- [x] Writer: `write_string_no_nul(std::string_view)` (legacy
      `packet_append_ntstring` parity, no NUL, empty = no-op).
- [x] Writer: `reserve(std::size_t)` -> offset.
- [x] Writer: `patch_u8(off, v)` / `patch_le<T>(off, v)` /
      `patch_be<T>(off, v)` with `OutOfRange` failure.
- [x] +4 Catch2 cases / +31 assertions in
      `tests/unit/protocol/common/writer_test.cpp` (green).
- [ ] Optional `Reader::read_lstring()` — defer until needed.
- [~] Per-protocol `header.hpp` helpers — DEFERRED.
      `codec.hpp` in each module (`file`, `d2cs`, `udp`) already
      exposes typed header structs + decode/encode. Revisit only if
      a port hits a gap.

## E.2  send_packet bridge

- [x] `send_packet_bridge.hpp` (header, ABI + C++ install API).
- [x] `send_packet_bridge.cpp` (dispatcher, no legacy includes,
      compiles in WITH_BNETD=OFF).
- [x] `send_packet_bridge_link.cpp` (raw-class `t_packet` +
      `conn_push_outqueue`, only in `integration_legacy_bnetd_linked`).
- [x] 8 Catch2 cases /
      `test_integration_legacy_bnetd_send_packet_bridge` (green in
      Dockerfile.v3 v3-test).
- [x] Wire `install_legacy_send_packet_handler()` into
      `install_v3_handlers.cpp` (exposed via
      `install_send_packet_handler()`; called unconditionally from
      `bnetd/server.cpp` under `PVPGN_V3_BNETD_INTEGRATION`).
- [ ] First strangler-fig call site (gated by
      `PVPGN_V3_BNETD_INTEGRATION`) — lands with first ported
      handler (see E.3).

## E.3  Per-module migration order

- [~] `handle_init.cpp` — first hotspot, gates every connection.
  - [x] V3 application module `application_init` with pure
        `dispatch_init_conn(InitConnRequest) -> InitConnResponse`
        owning the byte-1 -> connection-class decision table.
        Header [src/v3/application/init/include/application/init/init_conn_dispatch.hpp](src/v3/application/init/include/application/init/init_conn_dispatch.hpp).
  - [x] Catch2 suite
        [tests/unit/application/init/init_conn_dispatch_test.cpp](tests/unit/application/init/init_conn_dispatch_test.cpp)
        — 12 cases / 15 assertions; pins every accepted byte and
        verifies "exactly 5 bytes accepted, 251 rejected".
  - [x] `init_conn_bridge.hpp/cpp` exposing
        `pvpgn_v3_init_conn_decide(uint8_t cclass,
        uint8_t* out_decision)` C ABI.
        ([src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/init_conn_bridge.hpp](src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/init_conn_bridge.hpp),
        [src/v3/integration/legacy_bnetd/src/init_conn_bridge.cpp](src/v3/integration/legacy_bnetd/src/init_conn_bridge.cpp))
        Tests: 10 cases / 275 assertions including a full 0x00-0xff
        sweep ([tests/unit/integration/legacy_bnetd/init_conn_bridge_test.cpp](tests/unit/integration/legacy_bnetd/init_conn_bridge_test.cpp)).
  - [x] Strangler-fig call site in
        [src/bnetd/handle_init.cpp](src/bnetd/handle_init.cpp) gated
        by `PVPGN_V3_BNETD_INTEGRATION` (observer-only parity
        check: forward-declared `pvpgn_v3_init_conn_decide`,
        compares to legacy table, logs disagreement via
        `eventlog_level_warn`). v3 does not yet apply the decision.
  - [x] **Authoritative apply path**: v3 owns the per-class state
        transition (`conn_set_state` + `conn_set_class`) plus the
        D2CS_BNETD realmlist check and `handle_d2cs_init` call.
    - C ABI `pvpgn_v3_init_conn_apply(void*, uint8_t) -> int`
      returning 1 (handled), 0 (declined), -1 (handled-but-failed).
    - Dispatcher in [src/v3/integration/legacy_bnetd/src/init_conn_bridge.cpp](src/v3/integration/legacy_bnetd/src/init_conn_bridge.cpp);
      legacy-aware sink in
      [src/v3/integration/legacy_bnetd/src/init_conn_bridge_link.cpp](src/v3/integration/legacy_bnetd/src/init_conn_bridge_link.cpp).
    - Installer `install_init_conn_apply_handler()` in
      [src/v3/integration/legacy_bnetd/src/install_v3_handlers.cpp](src/v3/integration/legacy_bnetd/src/install_v3_handlers.cpp);
      called unconditionally from
      [src/bnetd/server.cpp](src/bnetd/server.cpp) under
      `PVPGN_V3_BNETD_INTEGRATION`.
    - Legacy `handle_init_packet` now: validates -> parity-log ->
      `pvpgn_v3_init_conn_apply(c, cclass)` -> on `1` break; on
      `-1` return -1; on `0` fall through to the (now redundant in
      combined builds) legacy switch which is retained as a safety
      net and as the only path in legacy-only builds.
    - 6 new Catch2 cases / +19 assertions covering the apply
      dispatcher: no-handler, null-conn, rejected bytes skip the
      handler, accepted forwarding, -1/0/1 return propagation.
  - [ ] Once parity log goes silent across a full run, remove the
        legacy switch and require `PVPGN_V3_BNETD_INTEGRATION` to
        build bnetd.
- [~] bnet auth/login (`handle_bnet_packet` auth slice).
  - [x] **AuthReply1 (0x07)** — first real send_packet_bridge consumer.
    - Fixed v3 `protocol::bnet::encode(AuthReply1)` to emit the
      legacy on-wire layout: header + u32 message + optional
      filename + 2 always-emitted trailing empty strings. Removed
      the unused `unknown` field; updated 2 round-trip tests.
    - New byte-parity Catch2 test covers BADVERSION, OK (no
      filename), and UPDATE-with-filename against fixed reference
      bytes from `bnet_protocol.h`.
    - New bridge
      [src/v3/integration/legacy_bnetd/src/send_authreply1_bridge.cpp](src/v3/integration/legacy_bnetd/src/send_authreply1_bridge.cpp)
      exposes
      `int pvpgn_v3_send_authreply1(void*, uint32_t message,
        char const* mpqfilename)`
      returning 1/0/-1. Builds bytes via the codec, ships via
      `pvpgn_v3_send_packet_try`.
    - 7 bridge unit tests (18 assertions) cover: no-handler,
      null-conn, BADVERSION/OK/UPDATE byte parity, handler decline
      / failure propagation.
    - Strangler-fig in `_client_authreq1`
      ([src/bnetd/handle_bnet.cpp](src/bnetd/handle_bnet.cpp))
      under `PVPGN_V3_BNETD_INTEGRATION` for both reply sites
      (`send_failed_packet` BADVERSION lambda + OK/UPDATE final
      emit). Legacy path retained as fallback; v3 path runs first
      and returns early on success.
  - [x] **AuthReply_109 (0x51)** — D2/LoD 1.09 auth reply.
    - Wire layout: header + u32 message + optional filename + ""\0
      (single trailing empty). Encoded directly via `Writer` in
      [src/v3/integration/legacy_bnetd/src/send_authreply109_bridge.cpp](src/v3/integration/legacy_bnetd/src/send_authreply109_bridge.cpp)
      (NOT routed through a `messages.hpp` struct because
      `kSidAuthCheck = 0x51` already names the newer-protocol
      AuthCheck and a second logical name for the same byte would
      pollute the `decode_server` dispatcher).
    - C ABI:
      `int pvpgn_v3_send_authreply109(void*, uint32_t message,
        char const* mpqfilename)` returning 1/0/-1.
    - 8 bridge tests / 21 assertions cover: no-handler, null-conn,
      OK / BADVERSION / UPDATE byte parity, empty-vs-null filename
      treated identically, handler decline / failure propagation.
    - Strangler-fig in `_client_authreq109`
      ([src/bnetd/handle_bnet.cpp](src/bnetd/handle_bnet.cpp)) for
      both reply sites; legacy path retained as fallback.
  - [x] **AuthInfo reply (SERVER_AUTHREQ_109, 0x50)** -- SID_AUTH_INFO
        server response to client_auth_info.
    - Wire layout: header + u32 logontype + u32 sessionkey + u32
      sessionnum + u64 FILETIME (low DWORD LE then high DWORD LE)
      + cstring mpq_filename + cstring equation + optional 128-byte
      zero "server signature" pad (W3 / W3XP only).
    - Reused existing v3 `AuthInfoReply` struct in
      [src/v3/protocol/bnet/include/protocol/bnet/messages.hpp](src/v3/protocol/bnet/include/protocol/bnet/messages.hpp);
      added a `std::vector<std::uint8_t> server_signature` tail and
      updated `encode` / `decode_auth_info_reply` in
      [src/v3/protocol/bnet/src/codec.cpp](src/v3/protocol/bnet/src/codec.cpp)
      to round-trip the optional pad.
    - New byte-parity codec test (`SID_AUTH_INFO (0x50) reply byte
      parity vs legacy`) covers standard-logon (43 bytes) and W3 +
      pad (154 bytes) variants.
    - Bridge:
      [src/v3/integration/legacy_bnetd/src/send_authinfo_reply_bridge.cpp](src/v3/integration/legacy_bnetd/src/send_authinfo_reply_bridge.cpp).
      C ABI:
      `int pvpgn_v3_send_authinfo_reply(void*, u32 logontype, u32
        server_token, u32 session_num, u64 timestamp, char const*
        mpq_filename, char const* checksum_formula,
        int include_w3_signature)` returning 1/0/-1.
    - 6 bridge tests / 174 assertions cover: no-handler, null-conn,
      standard-logon byte parity, W3 128-byte signature pad,
      null-string treated as empty, handler return propagation.
    - Strangler-fig in `_client_auth_info`
      ([src/bnetd/handle_bnet.cpp](src/bnetd/handle_bnet.cpp)):
      legacy build retained for parity / side effects; v3 attempt
      runs after legacy builds the rpacket and returns early on
      success (skipping `conn_push_outqueue`). `bn_long_get` reads
      the FILETIME out of the legacy-built packet so v3 sees the
      exact timestamp from `file_to_mod_time`.
  - [x] **LoginReply1/2 (0x29 / 0x3A)** -- old logon-system (OLS)
        success/fail replies.
    - Wire layout (LOGINREPLY1): header + u32 message (8 bytes,
      never appends a string).
    - Wire layout (LOGINREPLY2): header + u32 message + optional
      cstring reason. Legacy only appends a reason when the client
      supports the LOCKED variant (versionid >= 0xb).
    - Bridge:
      [src/v3/integration/legacy_bnetd/src/send_loginreply_bridge.cpp](src/v3/integration/legacy_bnetd/src/send_loginreply_bridge.cpp).
      Direct `protocol::Writer` (not routed through the v3
      `LoginReply1` / `LogonResponse2Reply` codec entries, which
      have their own trailing-string heuristics that don't match
      legacy exactly).
    - C ABI:
      `int pvpgn_v3_send_loginreply1(void*, u32 message)` and
      `int pvpgn_v3_send_loginreply2(void*, u32 message,
        char const* reason)` -- empty/null `reason` emits no
      trailing cstring.
    - 10 bridge tests / 45 assertions cover: no-handler, null-conn,
      SUCCESS / FAIL byte parity for both 0x29 and 0x3A, empty-vs-null
      reason treated identically, LOCKED + reason cstring (15-byte
      packet), handler return propagation.
    - Strangler-fig in `_client_loginreq1`
      ([src/bnetd/handle_bnet.cpp](src/bnetd/handle_bnet.cpp)):
      both reply sites (early too-many-logins + end success path);
      message read via `bn_int_get` from the legacy-built rpacket so
      all branch logic stays intact.
    - Strangler-fig in `_client_loginreq2`: end-of-handler push only,
      and only when `packet_get_size(rpacket) == sizeof(t_server_loginreply2)`
      (i.e. no `packet_append_string` was made). Legacy reason-string
      paths fall back to the legacy emit so the appended bytes are
      preserved verbatim.
  - [x] **LoginReplyW3 (0x53)** -- NLS / SRP step A reply.
    - Wire layout: header + u32 message + 32 bytes salt + 32 bytes
      server_public_key (72 bytes total, fixed).
    - Reused existing v3 `LoginW3Reply` codec
      ([src/v3/protocol/bnet/src/codec.cpp](src/v3/protocol/bnet/src/codec.cpp)).
    - Bridge:
      [src/v3/integration/legacy_bnetd/src/send_loginreply_w3_bridge.cpp](src/v3/integration/legacy_bnetd/src/send_loginreply_w3_bridge.cpp).
      C ABI:
      `int pvpgn_v3_send_loginreply_w3(void*, u32 message,
        u8 const* salt, u8 const* server_public_key)`. Either array
      may be `nullptr` -> 32 zero bytes (matches legacy BADACCT path).
    - 5 bridge tests / 153 assertions cover: no-handler, null-conn,
      BADACCT zero-padded 72-byte parity, SUCCESS with arbitrary
      salt + B bytes, handler return propagation.
    - Strangler-fig in `_client_loginreqw3`
      ([src/bnetd/handle_bnet.cpp](src/bnetd/handle_bnet.cpp)) at the
      single end-of-handler push site: reads message/salt/B back out
      of the legacy-built rpacket, ships via bridge, skips legacy
      push on success.
  - [x] **LogonProofReply (0x54)** -- NLS / SRP step M1/M2 reply.
    - Wire layout: header + u32 response + 20-byte server password
      proof. CUSTOM-lock branch appends a NUL-terminated reason
      string via `packet_append_string`.
    - Direct-Writer bridge (no codec entry added):
      [src/v3/integration/legacy_bnetd/src/send_logonproof_reply_bridge.cpp](src/v3/integration/legacy_bnetd/src/send_logonproof_reply_bridge.cpp).
      C ABI:
      `int pvpgn_v3_send_logonproof_reply(void*, u32 response,
        u8 const* server_password_proof, char const* custom_reason)`.
      Null proof -> 20 zero bytes. Null / empty reason -> no trailing
      string appended.
    - 7 bridge tests / 72 assertions: no-handler, null-conn, BADPASS
      with zero proof (28 bytes), OK with explicit proof bytes,
      CUSTOM + "locked" string (35 bytes), empty-string reason
      treated as no reason, return propagation.
    - Strangler-fig in `_client_logonproofreq` at the end-of-handler
      push site, gated by
      `packet_get_size(rpacket) == sizeof(t_server_logonproofreply)`
      (CUSTOM-lock path with appended-reason falls back to legacy
      emit, mirroring the LoginReply2 pattern). On v3 success it
      also clears the client/server proofs and calls
      `clan_send_status_window` before returning.
  - [x] **CreateAcctReply1 / 2 / W3 (0x2a / 0x3d / 0x52)** -- account
    create replies. Identical 8-byte wire layout:
    `header(4) + u32 result`. Single bridge file with three ABIs:
    [src/v3/integration/legacy_bnetd/src/send_createacct_reply_bridge.cpp](src/v3/integration/legacy_bnetd/src/send_createacct_reply_bridge.cpp).
    13 bridge tests / 32 assertions across all three codes.
    Stranglers:
    - `_client_createacctreq1` at single end-of-handler push site.
    - `_client_createacctreq2` at single end-of-handler push site.
    - `_client_createaccountw3` at all three push sites (disabled,
      invalid-name, account-create-success branch).
  - [x] **IconReply (0x2d)** -- icons.bni filename + mtime.
    - Wire layout: header(4) + u64 timestamp (LE) + NUL-terminated
      filename.
    - Direct-Writer bridge
      ([src/v3/integration/legacy_bnetd/src/send_iconreply_bridge.cpp](src/v3/integration/legacy_bnetd/src/send_iconreply_bridge.cpp))
      with ABI
      `int pvpgn_v3_send_iconreply(void*, u64 timestamp,
        char const* filename)`. Null / empty filename -> single NUL
      (matches legacy `packet_append_string(rpacket, "")`).
    - 5 bridge tests / 21 assertions including parity vs the
      reference bytes documented in `bnet_protocol.h`
      (FF 2D 16 00 ... "icons.bni\0").
    - Strangler-fig in `_client_iconreq` at the single end-of-handler
      push site. Reads `bn_long_get(rpacket->u.server_iconreply.timestamp)`
      and the appended filename (if `packet_get_size > sizeof(t_server_iconreply)`)
      back out of the legacy-built rpacket, ships via bridge, skips
      legacy push on success.
  - [ ] Remaining `_client_*` handlers in handle_bnet.cpp
        (countryinfo / regsnoopreply / changegameport).
- [ ] chat / channel / message.
- [ ] game list / start / report.
- [ ] clan.
- [ ] `anongame.cpp` + `anongame_infos.cpp`.

For each module:

1. Identify reply shapes; add wire structs if still deferred.
2. Build replies with `protocol::Writer`; send via
   `pvpgn_v3_send_packet_try`.
3. Catch2 byte-parity test against legacy reference bytes.
4. Update Dockerfile.v3 v3-build + v3-test target lists.
5. Tick the box here.

## E.4  Acceptance criteria

- [ ] Init + auth handlers reach 100% v3-bytes (legacy `packet_*`
      call sites = 0 in those files).
- [ ] No regression in `docker build -f Dockerfile.v3 --target
      v3-test`.
- [ ] Legacy `packet.cpp/.h` + `queue.cpp/.h` still build (other
      handlers still use them).
- [ ] (Follow-up "Step 4.5") Delete `src/common/packet.{cpp,h}` and
      `src/common/queue.{cpp,h}`.

## Bnproxy (user-confirmed in scope)

- [ ] Audit `src/bnproxy/` packet usage (PROXY_FLAG_UDP +
      `packet_class_raw`).
- [ ] Route opaque pass-through via `send_packet_bridge`.
- [ ] Remove bnproxy's dependency on `src/common/packet.cpp`.

## 179 deferred bnet message structs

- [ ] Land incrementally alongside each protocol's codec port.
      Tracked per-module under E.3 rather than as a single block.
