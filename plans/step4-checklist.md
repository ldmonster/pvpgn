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
  - [x] **Decoder-only sweep — countryinfo (0x12) / regsnoopreply
        (0x18) / changegameport (0x45)**: these are inbound-only
        packets (no server reply), so no strangler-fig bridge is
        needed. Verified that `decode_countryinfo1`,
        `decode_regsnoop_reply` and `decode_netgameport` already
        exist in [src/v3/protocol/bnet/src/codec.cpp](src/v3/protocol/bnet/src/codec.cpp)
        and that round-trip Catch2 cases exist in
        [tests/unit/protocol/bnet/codec_test.cpp](tests/unit/protocol/bnet/codec_test.cpp).
        Added 3 new **byte-parity vs legacy** tests pinning the
        exact reference bytes from `src/common/bnet_protocol.h`:
        - SID_COUNTRYINFO1 (0x12): 57 bytes — header + systemtime
          (u64 LE) + localtime (u64 LE) + bias (i32 LE = -300) +
          3x langid (u32 LE) + 4 cstrings
          ("ena","61","AUS","Australia").
        - SID_REGSNOOPREPLY (0x18): 12 bytes — `FF 18 0C 00 00 00
          00 00 42 6F 62 00` (header + u32 unknown1 + "Bob\0").
        - SID_NETGAMEPORT / SID_CHANGEGAMEPORT (0x45): 6 bytes —
          `FF 45 06 00 E0 17` (header + u16 port = 6112).
        Final codec test count: 1188 assertions / 212 cases
        (+81 / +3 from this slice). Closes the handle_bnet
        decoder-only sweep — no production code change required.
- [~] chat / channel / message.
  - [x] **SID_CHATEVENT (0x0F) byte-parity test** for TALK subtype
        landed in [tests/unit/protocol/bnet/codec_test.cpp](tests/unit/protocol/bnet/codec_test.cpp).
        Pins legacy `t_server_message` layout: header + u32 type +
        u32 flags + u32 latency + u32 player_ip(=0) + u32
        account_num (legacy BIG-endian `0x0df0adba` -> wire
        `0d f0 ad ba`) + u32 reg_auth (legacy LITTLE-endian
        `0xBAADF00D` -> wire `0d f0 ad ba`) + cstring username +
        cstring text.
        **Parity quirk discovered**: v3 `encode(ChatEvent)` uses LE
        for both magic fields; to produce the same wire bytes the
        caller must set BOTH `acct_number` and `registration` to
        `0xBAADF00D`. The eventual chatevent bridge MUST follow
        this convention. Test value: TALK + "Bob" + "hi" = 35
        bytes; final codec count 1225 assertions / 213 cases.
  - [x] Bridge `pvpgn_v3_send_chatevent(void*, u32 event_type,
        u32 flags, u32 latency_ms, char const* username,
        char const* text)` — emits `acct_number`/`registration`
        as the magic `0xBAADF00D` constant per the parity test.
        [src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_chatevent_bridge.hpp](src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_chatevent_bridge.hpp)
        + [src/v3/integration/legacy_bnetd/src/send_chatevent_bridge.cpp](src/v3/integration/legacy_bnetd/src/send_chatevent_bridge.cpp).
        Tests: 6 cases / 22 assertions covering no-handler,
        null-conn, TALK byte parity (35 bytes), null
        username/text emission (30 bytes), JOIN with flags +
        latency (35 bytes), -1 return propagation.
        [tests/unit/integration/legacy_bnetd/send_chatevent_bridge_test.cpp](tests/unit/integration/legacy_bnetd/send_chatevent_bridge_test.cpp).
  - [x] **Strangler-fig in `message_send`** (option (b) from the
        prior deferral note): the legacy `message_bnet_format`
        switch is left intact, but right before
        `conn_push_outqueue` in `message_send`
        ([src/bnetd/message.cpp](src/bnetd/message.cpp)) we re-
        extract `type / flags / latency / username / text` from
        the just-built legacy `t_server_message` packet and ship
        the bytes via `pvpgn_v3_send_chatevent`. On v3 success
        (rc=1) the legacy push is skipped; on decline/failure
        (0/-1) we fall through to the legacy push (preserves
        legacy behaviour and observability while the v3 transport
        proves itself in production). Gated by
        `PVPGN_V3_BNETD_INTEGRATION`. The `extern "C"` forward
        decl lives at the top of `message.cpp` so no v3 header
        leaks into the legacy namespace. Verified: Windows
        `bnetd_legacy.vcxproj` `message.cpp` compiles clean (other
        Windows-only `mkdir`/`S_IRWXU` errors in `mail.cpp` /
        `userlog.cpp` are pre-existing and unrelated). Docker
        `v3-test` stack remains 1225 / 213 + bridge 22 / 6 green.
  - [x] **`application_chat` compose module** (deeper refactor;
        first slice landed): new pure v3 module
        [src/v3/application/chat/include/application/chat/chat_event_compose.hpp](src/v3/application/chat/include/application/chat/chat_event_compose.hpp)
        + [src/v3/application/chat/src/chat_event_compose.cpp](src/v3/application/chat/src/chat_event_compose.cpp).
        Takes a fully pre-resolved `ComposeRequest` (so the v3
        tree never touches legacy `conn_*` getters) and returns a
        `protocol::bnet::ChatEvent` with the legacy
        `0xBAADF00D` magic baked in. Implements all 16
        `LegacyMessageType` arms mirroring the legacy
        `message_bnet_format` switch (TALK/INFO/ERROR/
        CHANNELFULL/CHANNELRESTRICTED/JOIN/PART/ADDUSER/
        WHISPER/BROADCAST/CHANNEL/USERFLAGS/WHISPERACK/
        FRIENDWHISPERACK/CHANNELDOESNOTEXIST/EMOTE) including
        the `me==NULL`, `MF_X`, and `me==dst` rejection
        contracts. Tests: 14 cases / 67 assertions
        [tests/unit/application/chat/chat_event_compose_test.cpp](tests/unit/application/chat/chat_event_compose_test.cpp).
  - [x] **Compose bridge `pvpgn_v3_send_chatevent_compose`**: new
        `extern "C"` ABI in
        [src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_chatevent_compose_bridge.hpp](src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_chatevent_compose_bridge.hpp)
        + [src/v3/integration/legacy_bnetd/src/send_chatevent_compose_bridge.cpp](src/v3/integration/legacy_bnetd/src/send_chatevent_compose_bridge.cpp).
        Takes the pre-resolved fields, routes them through
        `application_chat::compose_chat_event`, encodes the
        resulting `ChatEvent`, and ships via
        `pvpgn_v3_send_packet_try`. Honours all compose
        rejection contracts (MF_X / me==NULL / me==dst / unknown
        type -> rc=0; transport failure -> rc=-1; success -> rc=1).
        Tests: 7 cases / 33 assertions
        [tests/unit/integration/legacy_bnetd/send_chatevent_compose_bridge_test.cpp](tests/unit/integration/legacy_bnetd/send_chatevent_compose_bridge_test.cpp).
  - [x] **Deeper strangler-fig in `message_send`**: replaced the
        option-(b) re-extraction with a compose-based fast-path
        that runs BEFORE `message_cache_lookup` for
        `conn_class_bnet` destinations
        ([src/bnetd/message.cpp](src/bnetd/message.cpp)). Resolves
        `chatcharname / chatname / playerinfo /
        channel_flags_bncflags / me_flags / me_latency / dst_eq_me`
        up-front from the legacy `t_message` + `me` + `dst`
        accessors and hands them to
        `pvpgn_v3_send_chatevent_compose`. On v3 success the
        entire legacy `message_bnet_format` + cache + push path
        is bypassed; on v3 decline / transport failure the legacy
        path runs unchanged. Only message types 0..15
        (adduser..emote) are routed; uniqueid/mode/kick/quit/
        nick/notice and the WOL/IRC extensions still legacy-only.
        Verified `message.cpp` compiles clean under MSVC (only the
        pre-existing unrelated `mkdir`/`S_IRWXU` errors in
        `mail.cpp` / `userlog.cpp` remain). Docker `v3-test`:
        codec 1225 / 213, chat_event_compose 67 / 14,
        send_chatevent_compose_bridge 33 / 7 — all green.
  - [x] **Retired non-compose `pvpgn_v3_send_chatevent` bridge**:
        the original byte-pass-through bridge has no remaining
        callers after the strangler-fig was upgraded to compose,
        so deleted `send_chatevent_bridge.{hpp,cpp}` and its test
        from the v3 tree, the cmake source list, the Dockerfile
        targets, and the legacy forward decl in `message.cpp`.
  - [x] **End-to-end roundtrip test** for the compose path:
        [tests/unit/integration/legacy_bnetd/send_chatevent_compose_e2e_test.cpp](tests/unit/integration/legacy_bnetd/send_chatevent_compose_e2e_test.cpp)
        drives the bridge with full `ComposeRequest` fixtures,
        captures the wire bytes via a fake sink, frames them
        through `protocol::parse_packet`, decodes via
        `pb::decode_server` and asserts the recovered `ChatEvent`
        matches expected for TALK / JOIN / CHANNEL /
        WHISPER (me=NULL fallback) / INFO / CHANNELFULL. 6 cases
        / 63 assertions. Proves caller -> compose -> encode ->
        wire -> decode parity end-to-end.
  - [x] SID_JOINCHANNEL / LEAVECHANNEL handlers (mostly side-effect
        only -- no direct server reply; channel JOIN events go out
        as SID_CHATEVENT subtype JOIN, covered by the bridge
        above). Round 15 (this session): added observation-only
        bridges `pvpgn_v3_joinchannel_try(conn, name, flag)` and
        `pvpgn_v3_leavechannel_try(conn)` in
        `src/v3/integration/legacy_bnetd/{include/integration/legacy_bnetd,src}/channel_state_bridge.{hpp,cpp}`.
        Bridges always return 0 (legacy retains full state
        ownership) and emit a structured `bridge_log_kv` line at
        Debug level. JOIN logs `{channel, flag (NORMAL/GENERIC/
        CREATE/?), raw}`; LEAVE logs no fields. Wired into
        `_client_joinchannel` and `_client_leavechannel` in
        `src/bnetd/handle_bnet.cpp` under `#ifdef
        PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `channel_state_bridge_test.cpp` uses a recording
        `core::ILogger` (full virtual override: log/log_kv/level/
        set_level) + `BridgeLoggerOverride` RAII to assert
        return-0 contract, null-conn no-op, flag-name table,
        null-channel safety, empty-field LEAVE line. 6 cases.
        Both docker builds green; v3 still 1225 assertions / 213
        cases + the new test as a separate binary; 4 e2e green.
  - [ ] SID_JOINCHANNEL / LEAVECHANNEL handlers placeholder (kept
        for any future state-machine slice promotion).
  - [x] SID_CHANNELLIST (0x0B) — server emits list of channels.
        Round 14 (this session): added
        [src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_channellist_bridge.hpp](src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_channellist_bridge.hpp)
        + [src/v3/integration/legacy_bnetd/src/send_channellist_bridge.cpp](src/v3/integration/legacy_bnetd/src/send_channellist_bridge.cpp)
        exposing `pvpgn_v3_send_channellist(conn, names, count)`. Wire
        layout: header(4) + N NUL-terminated channel names + trailing
        NUL. Legacy `_client_progident2` keeps building rpacket with
        all the existing filter logic; under
        `PVPGN_V3_BNETD_INTEGRATION` we re-walk the already-built
        rpacket's cstrings into a `std::vector<char const*>` and offer
        them to the bridge before falling through to the legacy
        outqueue. Tests:
        [tests/unit/integration/legacy_bnetd/send_channellist_bridge_test.cpp](tests/unit/integration/legacy_bnetd/send_channellist_bridge_test.cpp)
        -- 8 cases / 18 assertions: no-handler, null-conn, empty list
        (FF 0B 05 00 00), three-name parity, skip-null-and-empty
        entries, oversize-count reject, non-zero-with-null-array
        reject, downstream-return propagation. Dockerfile.v3
        build + test target lists updated. Legacy docker build PASSED,
        v3 docker build PASSED (1225 / 213 + 4 e2e).
- [~] game list / start / report. Round 16 (this session, partial):
        added observation-only bridge
        `pvpgn_v3_gamereport_try(conn, username, player_count)` in
        `src/v3/integration/legacy_bnetd/{include/integration/legacy_bnetd,src}/game_report_bridge.{hpp,cpp}`.
        Always returns 0; emits a single `bridge_log_kv` Debug line
        `gamereport intent observed` with fields
        `{user, player_count}`. Wired into `_client_gamereport` in
        `src/bnetd/handle_bnet.cpp` immediately after the legacy
        info-level eventlog, under `#ifdef
        PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `game_report_bridge_test.cpp` covers null-conn no-op,
        happy-path field set, null-username -> empty string_view,
        max-uint player_count formatting. 4 cases. Legacy + v3
        docker builds green; v3 still 1225 assertions / 213 cases +
        the new test binary; 4 e2e green. STILL TO DO in this
        slice: bridges for STARTGAME1/3/4, JOINGAME, GAMELISTREQ.

        Round 16 continuation (this session): added unified
        observation bridge
        `pvpgn_v3_startgame_try(conn, version, gamename, gameinfo,
        bngtype, status, flag, option)` in
        `src/v3/integration/legacy_bnetd/{include/integration/legacy_bnetd,src}/startgame_bridge.{hpp,cpp}`.
        Always returns 0; emits a single `bridge_log_kv` Debug line
        `startgame intent observed` with fields `{variant,
        version, game, bngtype, status, flag, option, infolen}`.
        Variant table: 1=STARTGAME1, 3=STARTGAME3, 4=STARTGAME4, ?
        otherwise. Hex fields rendered via `snprintf("%08x")`.
        Wired into all three legacy handlers
        `_client_startgame{1,3,4}` in `src/bnetd/handle_bnet.cpp`
        immediately after the existing legacy debug-level eventlog,
        under `#ifdef PVPGN_V3_BNETD_INTEGRATION`. STARTGAME1/3
        pass flag=0/option=0 (those variants don't carry them).
        Unit test `startgame_bridge_test.cpp`: null-conn no-op,
        variant-name table for 1/3/4/unknown, hex formatting +
        infolen, null-string safety. 4 cases. Legacy + v3 docker
        builds green; v3 still 1225 / 213 + new test binary; 4 e2e
        green. STILL TO DO in this slice: JOINGAME, GAMELISTREQ.

        Round 16 final (this session): added combined observation
        bridge with two entry points
        `pvpgn_v3_gamelistreq_try(conn, gamename, bngtype)` and
        `pvpgn_v3_joingame_try(conn, gamename)` in
        `src/v3/integration/legacy_bnetd/{include/integration/legacy_bnetd,src}/gamelist_join_bridge.{hpp,cpp}`.
        Always return 0. GAMELISTREQ logs
        `{scope (specific/public_list, derived from empty/non-empty
        gamename), game, bngtype (hex)}`. JOINGAME logs
        `{game}`. Wired into `_client_gamelistreq` right after
        `bngreqtype_to_gtype` (before `packet_create`) and into
        `_client_joingame` right after the legacy debug-level
        eventlog, under `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Unit
        test `gamelist_join_bridge_test.cpp`: null-conn no-ops
        (x2), public_list scope, specific scope, null-gamename
        defaulting to public_list, joingame happy path, joingame
        null gamename. 7 cases. Legacy + v3 docker builds green;
        v3 still 1225 / 213 + new test binary; 4 e2e green.

        Round 16 fully complete: GAME_REPORT, STARTGAME1/3/4,
        GAMELISTREQ, JOIN_GAME all have observation bridges.
- [x] game list / start / report -- completed in Round 16.
- [ ] clan.
- [~] `anongame.cpp` + `anongame_infos.cpp`. Round 17 (this
        session, partial): added observation bridge for SID_FINDANONGAME
        (0x44) sub-dispatch. New
        `pvpgn_v3_anongame_dispatch_try(conn, option)` in
        `src/v3/integration/legacy_bnetd/{include/integration/legacy_bnetd,src}/anongame_dispatch_bridge.{hpp,cpp}`.
        Always returns 0. Emits a structured Debug log line
        `anongame dispatch observed` with `{option (SEARCH/INFOS/
        CANCEL/PROFILE/AT_SEARCH/AT_INVITER_SEARCH/TOURNAMENT/
        PROFILE_CLAN/GET_ICON/SET_ICON/?), raw (hex)}` mirroring
        the constants in `src/common/anongame_protocol.h`. Wired
        at the top of `handle_anongame_packet` in
        `src/bnetd/handle_anongame.cpp` under `#ifdef
        PVPGN_V3_BNETD_INTEGRATION` (forward decl placed after
        `setup_after.h` so the legacy include order isn't
        disturbed). Unit test
        `anongame_dispatch_bridge_test.cpp`: null-conn no-op,
        full 10-value option-name table + unknown (?), hex raw
        formatting. 3 cases / 31 assertions. Legacy + v3 docker
        builds green; v3 still 1225 / 213 + the new test binary;
        4 e2e green. STILL TO DO in this slice: observation
        bridges for `handle_anongame_search` /
        `handle_anongame_join` entry points (or skip if
        higher-value slices land first), and an `anongame_infos`
        slice (config + lookup already has v3 facade
        `infra/legacy_config/anongame_infos_loader.cpp`).

        Round 17 continuation (this session): added observation
        bridge `pvpgn_v3_anongame_entry_try(conn, kind)` in
        `src/v3/integration/legacy_bnetd/{include/integration/legacy_bnetd,src}/anongame_entry_bridge.{hpp,cpp}`.
        Always returns 0. Emits a structured Debug log line
        `anongame entry observed` with `{kind}`. Wired into
        `handle_anongame_search` (kind="search") and
        `handle_anongame_join` (kind="join") in
        `src/bnetd/anongame.cpp` under `#ifdef
        PVPGN_V3_BNETD_INTEGRATION` (forward decl placed after
        `setup_after.h`). Unit test
        `anongame_entry_bridge_test.cpp`: null-conn no-op,
        kind="search"/"join" both logged, null kind -> "?". 3
        cases / 12 assertions. Legacy + v3 docker builds green;
        v3 still 1225 / 213 + the new test binary; 4 e2e green.
        STILL TO DO in this slice: `anongame_infos` runtime read
        facade (load is already covered by
        `anongame_infos_loader.cpp`).
    - [x] Round 18 -- observation bridge for `anongame_infos`
        lifecycle. Added
        `pvpgn_v3_anongame_infos_load_try(filename)` and
        `pvpgn_v3_anongame_infos_unload_try()` in
        `integration/legacy_bnetd/anongame_infos_bridge.{hpp,cpp}`,
        logging module `v3_anongame_infos_bridge` with messages
        `anongame_infos load observed` (field `file`) and
        `anongame_infos unload observed` (no fields). Wired into
        `src/bnetd/anongame_infos.cpp` `anongame_infos_load` and
        `anongame_infos_unload` under `#ifdef
        PVPGN_V3_BNETD_INTEGRATION` (forward decls at file scope
        after `setup_after.h`). Unit test
        `anongame_infos_bridge_test.cpp`: load with filename,
        load with null filename, unload. 3 cases / 13 assertions.
        Legacy + v3 docker builds green; v3 still
        1225 / 213 + the new test binary; 4 e2e green.
    - [x] Round 19 -- coalesced observation bridge for the 12
        runtime data getters in `anongame_infos.cpp`. Added
        `pvpgn_v3_anongame_infos_get_try(kind, arg0, arg1, arg2)`
        in
        `integration/legacy_bnetd/anongame_infos_get_bridge.{hpp,cpp}`,
        module `v3_anongame_infos_get_bridge`, message
        `anongame_infos get observed`. File-local helper
        `v3_anongame_infos_get_observe(kind, int, int, char*)`
        in `src/bnetd/anongame_infos.cpp` formats ints via
        `snprintf` and forwards the optional clienttag arg
        rendered via `tag_uint_to_str`. Wired at the top of
        `URL_get_URL`, `DESC_get_DESC`, `get_short_desc`,
        `get_long_desc`, `get_thumbsdown`, `get_ICON_REQ`,
        `get_ICON_REQ_TOURNEY`, `data_get_url`, `data_get_map`,
        `data_get_type`, `data_get_desc`, `data_get_ladr` under
        `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `anongame_infos_get_bridge_test.cpp`: kind+args, full
        args incl. clienttag string, null kind -> "?". 3 cases /
        16 assertions. Legacy + v3 docker builds green; v3 still
        1225 / 213 + the new test binary; 4 e2e green.
    - [x] Round 20 -- coalesced observation bridge for the 13
        SID_CLAN_* handlers in `handle_bnet.cpp`. Added
        `pvpgn_v3_clan_dispatch_try(conn, op)` in
        `integration/legacy_bnetd/clan_dispatch_bridge.{hpp,cpp}`,
        module `v3_clan_dispatch_bridge`, message
        `SID_CLAN_* dispatch observed`, field `{op}`. Wired at
        the top of `_client_claninforeq`,
        `_client_clanmemberlistreq`, `_client_clan_motdreq`,
        `_client_clan_motdchg`, `_client_clan_disbandreq`,
        `_client_clan_createreq`, `_client_clan_createinvitereq`,
        `_client_clan_createinvitereply`,
        `_client_clanmember_rankupdatereq`,
        `_client_clanmember_removereq`,
        `_client_clan_membernewchiefreq`,
        `_client_clan_invitereq`, `_client_clan_invitereply`
        under `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `clan_dispatch_bridge_test.cpp`: null-conn no-op, op
        logged, null op -> "?". 3 cases / 10 assertions. Legacy
        + v3 docker builds green; v3 still 1225 / 213 + the new
        test binary; 4 e2e green.
    - [x] Round 21 -- coalesced observation bridge for
        SID_FRIENDSLISTREQ + SID_FRIENDINFOREQ. Added
        `pvpgn_v3_friends_dispatch_try(conn, op)` in
        `integration/legacy_bnetd/friends_dispatch_bridge.{hpp,cpp}`,
        module `v3_friends_dispatch_bridge`, message
        `SID_FRIENDS_* dispatch observed`, field `{op}`. Wired
        at the top of `_client_friendslistreq` (op
        "friendslistreq") and `_client_friendinforeq` (op
        "friendinforeq") in `src/bnetd/handle_bnet.cpp` under
        `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `friends_dispatch_bridge_test.cpp`: null-conn no-op,
        each op logged, null op -> "?". 4 cases / 13 assertions.
        Legacy + v3 docker builds green; v3 still 1225 / 213 +
        the new test binary; 4 e2e green.
    - [x] Round 22 -- coalesced observation bridge for the
        SID_AUTH_* family (`_client_auth_info`,
        `_client_authreq1`, `_client_authreq109`). Added
        `pvpgn_v3_auth_dispatch_try(conn, op)` in
        `integration/legacy_bnetd/auth_dispatch_bridge.{hpp,cpp}`,
        module `v3_auth_dispatch_bridge`, message
        `SID_AUTH_* dispatch observed`, field `{op}`. Wired at
        the top of each of the three handlers in
        `src/bnetd/handle_bnet.cpp` under
        `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `auth_dispatch_bridge_test.cpp`: null-conn no-op, each
        op logged, null op -> "?". 5 cases / 16 assertions.
        Legacy + v3 docker builds green; v3 still 1225 / 213 +
        the new test binary; 4 e2e green.
    - [x] Round 23 -- coalesced observation bridge for the
        keepalive family: SID_PING (`_client_pingreq`) and
        SID_ECHO reply (`_client_echoreply`). Added
        `pvpgn_v3_keepalive_dispatch_try(conn, op)` in
        `integration/legacy_bnetd/keepalive_dispatch_bridge.{hpp,cpp}`,
        module `v3_keepalive_dispatch_bridge`, message
        `keepalive dispatch observed`, field `{op}`, log level
        Trace (high call frequency). Wired into both handlers in
        `src/bnetd/handle_bnet.cpp` under
        `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `keepalive_dispatch_bridge_test.cpp`: null-conn no-op,
        pingreq + echoreply logged, null op -> "?". 4 cases / 13
        assertions. Legacy + v3 docker builds green; v3 still
        1225 / 213 + the new test binary; 4 e2e green.
    - [x] Round 24 -- coalesced observation bridge for the
        realm family: SID_QUERYREALMS (`_client_realmlistreq`),
        SID_QUERYREALMS2 (`_client_realmlistreq110`) and
        SID_LOGONREALMEX (`_client_realmjoinreq109`). Added
        `pvpgn_v3_realm_dispatch_try(conn, op)` in
        `integration/legacy_bnetd/realm_dispatch_bridge.{hpp,cpp}`,
        module `v3_realm_dispatch_bridge`, message
        `realm dispatch observed`, field `{op}`. Wired into all
        three handlers in `src/bnetd/handle_bnet.cpp` under
        `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `realm_dispatch_bridge_test.cpp`: null-conn no-op, each
        op logged, null op -> "?". 5 cases / 16 assertions.
        Legacy + v3 docker builds green; v3 still 1225 / 213 +
        the new test binary; 4 e2e green.
    - [x] Round 25 -- coalesced observation bridge for the
        account-management family: SID_CREATEACCOUNT
        (`_client_createaccountw3`, `_client_createacctreq1`,
        `_client_createacctreq2`), SID_CHANGEPASSWORD
        (`_client_changepassreq`), SID_SETEMAIL reply
        (`_client_setemailreply`) and SID_CHANGEEMAIL
        (`_client_changeemailreq`). Added
        `pvpgn_v3_account_dispatch_try(conn, op)` in
        `integration/legacy_bnetd/account_dispatch_bridge.{hpp,cpp}`,
        module `v3_account_dispatch_bridge`, message
        `account dispatch observed`, field `{op}`. Wired into
        all six handlers in `src/bnetd/handle_bnet.cpp` under
        `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `account_dispatch_bridge_test.cpp`: null-conn no-op,
        single op logged, multi-op sequence, null op -> "?". 4
        cases / 19 assertions. Legacy + v3 docker builds green;
        v3 still 1225 / 213 + the new test binary; 4 e2e green.
    - [x] Round 26 -- coalesced observation bridge for the
        profile / stats family: SID_READUSERDATA
        (`_client_profilereq`), stats read (`_client_statsreq`)
        and SID_WRITEUSERDATA (`_client_statsupdate`). Added
        `pvpgn_v3_profile_dispatch_try(conn, op)` in
        `integration/legacy_bnetd/profile_dispatch_bridge.{hpp,cpp}`,
        module `v3_profile_dispatch_bridge`, message
        `profile dispatch observed`, field `{op}`. Wired into
        all three handlers in `src/bnetd/handle_bnet.cpp` under
        `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `profile_dispatch_bridge_test.cpp`: null-conn no-op,
        each op logged, null op -> "?". 5 cases / ~16
        assertions. Legacy + v3 docker builds green; v3 still
        1225 / 213 + the new test binary; 4 e2e green.
    - [x] Round 27 -- coalesced observation bridge for the
        ladder family: CLIENT_LADDERREQ (`_client_ladderreq`)
        and CLIENT_LADDERSEARCHREQ (`_client_laddersearchreq`).
        Added `pvpgn_v3_ladder_dispatch_try(conn, op)` in
        `integration/legacy_bnetd/ladder_dispatch_bridge.{hpp,cpp}`,
        module `v3_ladder_dispatch_bridge`, message
        `ladder dispatch observed`, field `{op}`. Wired into
        both handlers in `src/bnetd/handle_bnet.cpp` under
        `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `ladder_dispatch_bridge_test.cpp`: null-conn no-op,
        each op logged, null op -> "?". 4 cases / ~13
        assertions. Legacy + v3 docker builds green; v3 still
        1225 / 213 + the new test binary; 4 e2e green.
    - [x] Round 28 -- coalesced observation bridge for the D2
        character family: `_client_charlistreq` (CLIENT_UNKNOWN_37
        / SID_CHARLIST). Investigated `_client_unknown39` and
        SID_NEWS_INFO -- both absent or stub in legacy bnetd so
        excluded. Added `pvpgn_v3_d2_character_dispatch_try(conn,
        op)` in
        `integration/legacy_bnetd/d2_character_dispatch_bridge.{hpp,cpp}`,
        module `v3_d2_character_dispatch_bridge`, message
        `d2 character dispatch observed`, field `{op}`. Wired
        into the single handler in `src/bnetd/handle_bnet.cpp`
        under `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `d2_character_dispatch_bridge_test.cpp`: null-conn no-op,
        charlistreq logged, null op -> "?", empty op -> "?". 4
        cases / ~12 assertions. Legacy + v3 docker builds green;
        v3 still 1225 / 213 + the new test binary; 4 e2e green.
    - [x] Round 29 -- coalesced observation bridge for the misc
        telemetry family: `_client_udpok`,
        `_client_fileinforeq`, `_client_extrawork` and
        `_client_crashdump`. Added
        `pvpgn_v3_telemetry_dispatch_try(conn, op)` in
        `integration/legacy_bnetd/telemetry_dispatch_bridge.{hpp,cpp}`,
        module `v3_telemetry_dispatch_bridge`, message
        `telemetry dispatch observed`, field `{op}`. Wired into
        all four handlers in `src/bnetd/handle_bnet.cpp` under
        `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `telemetry_dispatch_bridge_test.cpp`: null-conn no-op,
        each op logged, null op -> "?". 6 cases / ~19
        assertions. Legacy + v3 docker builds green; v3 still
        1225 / 213 + the new test binary; 4 e2e green.
    - [x] Round 30 -- coalesced observation bridge for the ad
        family: `_client_adreq`, `_client_adack`,
        `_client_adclick` and `_client_adclick2`. Added
        `pvpgn_v3_ad_dispatch_try(conn, op)` in
        `integration/legacy_bnetd/ad_dispatch_bridge.{hpp,cpp}`,
        module `v3_ad_dispatch_bridge`, message
        `ad dispatch observed`, field `{op}`. Wired into all
        four handlers in `src/bnetd/handle_bnet.cpp` under
        `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `ad_dispatch_bridge_test.cpp`: null-conn no-op, each op
        logged, null op -> "?". 6 cases / ~19 assertions. Legacy
        + v3 docker builds green; v3 still 1225 / 213 + the new
        test binary; 4 e2e green.
    - [x] Round 31 -- coalesced observation bridge for the
        progident family: `_client_progident`,
        `_client_progident2` and `_client_changeclient`. Added
        `pvpgn_v3_progident_dispatch_try(conn, op)` in
        `integration/legacy_bnetd/progident_dispatch_bridge.{hpp,cpp}`,
        module `v3_progident_dispatch_bridge`, message
        `progident dispatch observed`, field `{op}`. Wired into
        all three handlers in `src/bnetd/handle_bnet.cpp` under
        `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `progident_dispatch_bridge_test.cpp`: null-conn no-op,
        each op logged, null op -> "?". 5 cases / ~16
        assertions. Legacy + v3 docker builds green; v3 still
        1225 / 213 + the new test binary; 4 e2e green.
    - [x] Round 32 -- coalesced observation bridge for the
        gameport / mapauth family: `_client_changegameport`,
        `_client_mapauthreq1` and `_client_mapauthreq2`. Added
        `pvpgn_v3_gameport_dispatch_try(conn, op)` in
        `integration/legacy_bnetd/gameport_dispatch_bridge.{hpp,cpp}`,
        module `v3_gameport_dispatch_bridge`, message
        `gameport dispatch observed`, field `{op}`. Wired into
        all three handlers in `src/bnetd/handle_bnet.cpp` under
        `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `gameport_dispatch_bridge_test.cpp`: null-conn no-op,
        each op logged, null op -> "?". 5 cases / ~16
        assertions. Legacy + v3 docker builds green; v3 still
        1225 / 213 + the new test binary; 4 e2e green.
    - [x] Round 33 -- coalesced observation bridge for the cdkey
        family: `_client_cdkey`, `_client_cdkey2` and
        `_client_cdkey3`. Added
        `pvpgn_v3_cdkey_dispatch_try(conn, op)` in
        `integration/legacy_bnetd/cdkey_dispatch_bridge.{hpp,cpp}`,
        module `v3_cdkey_dispatch_bridge`, message
        `cdkey dispatch observed`, field `{op}`. Wired into all
        three handlers in `src/bnetd/handle_bnet.cpp` under
        `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `cdkey_dispatch_bridge_test.cpp`: null-conn no-op, each
        op logged, null op -> "?". 5 cases / ~16 assertions.
        Legacy + v3 docker builds green; v3 still 1225 / 213 +
        the new test binary; 4 e2e green.
    - [x] Round 34 -- coalesced observation bridge for the
        password / email SRP family. Original scope included
        `changepassreq`, `changeemailreq`, `setemailreply` but
        these are already covered by the Round 25 account bridge
        (verified in handle_bnet.cpp). Remaining password-flow
        handlers added here: `_client_passchangereq`,
        `_client_passchangeproofreq` and
        `_client_getpasswordreq`. Added
        `pvpgn_v3_passemail_dispatch_try(conn, op)` in
        `integration/legacy_bnetd/passemail_dispatch_bridge.{hpp,cpp}`,
        module `v3_passemail_dispatch_bridge`, message
        `passemail dispatch observed`, field `{op}`. Wired into
        all three handlers in `src/bnetd/handle_bnet.cpp` under
        `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `passemail_dispatch_bridge_test.cpp`: null-conn no-op,
        each op logged, null op -> "?". 5 cases / ~16
        assertions. Legacy + v3 docker builds green; v3 still
        1225 / 213 + the new test binary; 4 e2e green.
    - [x] Round 35 -- coalesced observation bridge for the early
        handshake family: `_client_motdw3`, `_client_compinfo1`,
        `_client_compinfo2` and `_client_countryinfo1`. Added
        `pvpgn_v3_handshake_dispatch_try(conn, op)` in
        `integration/legacy_bnetd/handshake_dispatch_bridge.{hpp,cpp}`,
        module `v3_handshake_dispatch_bridge`, message
        `handshake dispatch observed`, field `{op}`. Wired into
        all four handlers in `src/bnetd/handle_bnet.cpp` under
        `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `handshake_dispatch_bridge_test.cpp`: null-conn no-op,
        each op logged, null op -> "?". 6 cases / ~19
        assertions. Legacy + v3 docker builds green; v3 still
        1225 / 213 + the new test binary; 4 e2e green.
    - [x] Round 36 -- coalesced observation bridge for the stub /
        unknown / debug-only family: `_client_unknown_1b`,
        `_client_unknown2b`, `_client_unknown39`,
        `_client_regsnoopreply` and `_client_readmemory`. Added
        `pvpgn_v3_stub_dispatch_try(conn, op)` in
        `integration/legacy_bnetd/stub_dispatch_bridge.{hpp,cpp}`,
        module `v3_stub_dispatch_bridge`, message
        `stub dispatch observed`, field `{op}`. Wired into all
        five handlers in `src/bnetd/handle_bnet.cpp` under
        `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Unit test
        `stub_dispatch_bridge_test.cpp`: null-conn no-op, each
        op logged, null op -> "?". 7 cases / ~22 assertions.
        Legacy + v3 docker builds green; v3 still 1225 / 213 +
        the new test binary; 4 e2e green.
    - [x] Round 37 -- coalesced observation bridge for the bnetd
        file-transfer dispatcher in `src/bnetd/handle_file.cpp`:
        `CLIENT_FILE_REQ`, `CLIENT_FILE_REQ2` and
        `CLIENT_FILE_REQ3`. Added
        `pvpgn_v3_file_dispatch_try(conn, op)` in
        `integration/legacy_bnetd/file_dispatch_bridge.{hpp,cpp}`,
        module `v3_file_dispatch_bridge`, message
        `file dispatch observed`, field `{op}`. Wired into all
        three cases inside `handle_file_packet` under
        `#ifdef PVPGN_V3_BNETD_INTEGRATION` (forward-decl after
        `setup_after.h`). Unit test
        `file_dispatch_bridge_test.cpp`: null-conn no-op, each
        op logged, null op -> "?". 5 cases / ~16 assertions.
        Legacy + v3 docker builds green; v3 still 1225 / 213 +
        the new test binary; 4 e2e green.

For each module:

1. Identify reply shapes; add wire structs if still deferred.
2. Build replies with `protocol::Writer`; send via
   `pvpgn_v3_send_packet_try`.
3. Catch2 byte-parity test against legacy reference bytes.
4. Update Dockerfile.v3 v3-build + v3-test target lists.
5. Tick the box here.

## E.4  Acceptance criteria

### Audit: residual `packet_create()` call sites in `src/bnetd/`

Snapshot (this session) — total **156 sites across 19 files**:

|  Count | File                              |
|-------:|-----------------------------------|
|     68 | handle_bnet.cpp                    |
|     11 | handle_bot.cpp                     |
|     11 | handle_telnet.cpp                  |
|      9 | anongame.cpp                       |
|      9 | server.cpp                         |
|      9 | clan.cpp                           |
|      8 | handle_anongame.cpp                |
|      5 | message.cpp                        |
|      5 | handle_d2cs.cpp                    |
|      5 | connection.cpp                     |
|      4 | command.cpp                        |
|      3 | irc.cpp                            |
|      2 | file.cpp                           |
|      2 | anongame_infos.cpp                 |
|      1 | udptest_send.cpp                   |
|      1 | handle_file.cpp                    |
|      1 | handle_apireg.cpp                  |
|      1 | handle_wol.cpp                     |
|      1 | anongame_wol.cpp                   |

E.4 cutover (delete `src/common/packet.{cpp,h}` and
`src/common/queue.{cpp,h}`) is gated on bringing each of these to
**zero** OR moving residual callers onto v3 bridges. The remaining
E.3 modules (chat, game-list, clan, anongame, bot/telnet/wol/irc
protocol handlers, d2cs/file/apireg/udptest sidecars, plus
server.cpp/connection.cpp internal users) **must land first**.

- [ ] Init + auth handlers reach 100% v3-bytes (legacy `packet_*`
      call sites = 0 in those files).
- [ ] No regression in `docker build -f Dockerfile.v3 --target
      v3-test`.
- [ ] Legacy `packet.cpp/.h` + `queue.cpp/.h` still build (other
      handlers still use them).
- [ ] (Follow-up "Step 4.5") Delete `src/common/packet.{cpp,h}` and
      `src/common/queue.{cpp,h}`.

## Bnproxy (user-confirmed in scope)

- [x] Decision: **dropped**, not migrated. The directory was marked
      `DEPRECATED.md` (excluded from the build since PvPGN 4.0) and
      [plans/refactoring-plan-legacy-tools.md](refactoring-plan-legacy-tools.md)
      "Step 6" already prescribed deletion. `src/bnproxy/` and
      `man/bnproxy.1` removed; `man/CMakeLists.txt` install list
      updated. No v3 port needed -- modern proxying is delegated to
      external tools (HAProxy / nginx) per the deprecation notice.
- [x] Audit `src/bnproxy/` packet usage (`PROXY_FLAG_UDP` +
      `packet_class_raw`) -- N/A, code removed.
- [x] Route opaque pass-through via `send_packet_bridge` -- N/A.
- [x] Remove bnproxy's dependency on `src/common/packet.cpp` -- N/A.

## 179 deferred bnet message structs

- [ ] Land incrementally alongside each protocol's codec port.
      Tracked per-module under E.3 rather than as a single block.

## Side track: legacy `src/test/` retirement

- [x] Ported `src/test/bigint.cpp` (assert-based) to Catch2 at
      [tests/unit/infra/legacy_crypto/bigint_test.cpp](../tests/unit/infra/legacy_crypto/bigint_test.cpp).
      11 test cases / 36 assertions covering BigInt constructors,
      getData, comparison, add/sub/mul/div/mod, random, powm.
      Documented the legacy `assert(data[i] = data4[i])` typo that
      made the round-trip block a no-op (kept the contract honest:
      only checks the buffer is non-null + the 3/2/1-byte truncation
      round-trips).
- [x] Ported `src/test/bnetsrp3_test.cpp` to Catch2 at
      [tests/unit/infra/legacy_crypto/bnetsrp3_test.cpp](../tests/unit/infra/legacy_crypto/bnetsrp3_test.cpp).
      Single deterministic SRP3 exchange with fixed salt, verifying
      client and server agree on the hashed shared secret.
- [x] Wired both tests under
      [tests/unit/infra/legacy_crypto/CMakeLists.txt](../tests/unit/infra/legacy_crypto/CMakeLists.txt)
      with `pvpgn_v3_add_test(... DEPS common)`, gated on
      `if(TARGET common)`.
- [x] Added `test_common_bigint` + `test_common_bnetsrp3` to
      [Dockerfile.v3](../Dockerfile.v3) v3-build target list and
      v3-test runner.
- [x] Deleted `src/test/` directory.
- [x] Removed `add_subdirectory(test)` from
      [src/CMakeLists.txt](../src/CMakeLists.txt) (with comment
      pointer to the v3 ports).
- [x] Verified green in docker v3-test: bigint 36/11, bnetsrp3 1/1,
      full suite still green.

## Side track: vendored `src/json/` retirement

- [x] Added `"nlohmann-json"` to [vcpkg.json](../vcpkg.json).
- [x] Top-level [CMakeLists.txt](../CMakeLists.txt) gained a
      `find_package(nlohmann_json CONFIG QUIET)` block (inside the
      `PVPGN_BUILD_LEGACY` arm) mirroring the existing `fmt` pattern,
      with a `FetchContent` fallback pinned to `v3.11.3` for Alpine
      / no-vcpkg checkouts.
- [x] Switched [src/bnetd/adbanner.cpp](../src/bnetd/adbanner.cpp)
      and [src/bnetd/versioncheck.cpp](../src/bnetd/versioncheck.cpp)
      from `#include "json/json.hpp"` to `#include
      <nlohmann/json.hpp>`.
- [x] [src/bnetd/CMakeLists.txt](../src/bnetd/CMakeLists.txt):
      dropped `../json/json.hpp` from the bnetd_legacy sources and
      added `nlohmann_json::nlohmann_json` to the PUBLIC link line.
- [x] Deleted `src/json/` directory.
- [x] Verified docker `Dockerfile` (Alpine, FetchContent path) builds
      `bnetd_legacy` + `bnetd` green; `Dockerfile.v3` v3-test suite
      remains all-green.

## Side track: `src/bnpass/` retirement

- [x] Created [src/v3/tools/bnpass/](../src/v3/tools/bnpass/) with
      `src/main.cpp` + `CMakeLists.txt`. Single executable
      `pvpgn-bnpass` covers both legacy modes -- default emits the
      `passhash1` attribute line; `--sha1` switches to a true-SHA-1
      digest (replaces the legacy `sha1hash` CLI).
- [x] Routes all hashing through `infra_crypto`
      (`pvpgn::v3::infra::crypto::{blizzard_hash,sha1,to_hex}`) -- no
      legacy `common` / `compat` linkage.
- [x] Wired into [src/v3/CMakeLists.txt](../src/v3/CMakeLists.txt)
      next to `tools/conf_converter` and added to the
      [Dockerfile.v3](../Dockerfile.v3) v3-build target list.
- [x] Removed `add_subdirectory(bnpass)` from
      [src/CMakeLists.txt](../src/CMakeLists.txt) with a pointer
      comment to the v3 location.
- [x] Deleted `src/bnpass/` directory.
- [x] Functional verification in docker:
      `pvpgn-bnpass test` -> `"BNET\\acct\\passhash1"="d9baa5b649fe62ad9e384c5c31a27e739c5f4165"`
      and `pvpgn-bnpass --sha1 test` ->
      `sha1 hash = a94a8fe5ccb19ba61c4c0873d391e987982fbbd3`
      (matches canonical SHA-1 of `test`).
- [x] Legacy `Dockerfile` build-plain stage still builds all other
      legacy targets green (bnetd, d2cs, d2dbs, bntrackd, bnchat,
      bnbot, bnftp, bnstat, bnilist, bni2tga, bniextract, bnibuild,
      tgainfo).

## Side track: `src/bnproxy/` retirement

- [x] Confirmed deprecated since 4.0 (per
      `src/bnproxy/DEPRECATED.md`) and earmarked for deletion by
      [plans/refactoring-plan-legacy-tools.md](refactoring-plan-legacy-tools.md)
      "Step 6". Deleted `src/bnproxy/` outright; removed
      `man/bnproxy.1` and dropped it from
      [man/CMakeLists.txt](../man/CMakeLists.txt). No v3 port --
      modern proxying is delegated to HAProxy / nginx.

## Side track: `src/bniutils/` relocation to v3

- [x] Moved all `src/bniutils/` sources into
      [src/v3/tools/bniutils/](../src/v3/tools/bniutils/) (`bni.cpp`,
      `bni.h`, `bni2tga.cpp`, `bnibuild.cpp`, `bniextract.cpp`,
      `bnilist.cpp`, `fileio.cpp`, `fileio.h`, `tga.cpp`, `tga.h`,
      `tgainfo.cpp`). Sources kept verbatim; build moves under the
      v3 cmake tree.
- [x] Wrote [src/v3/tools/bniutils/CMakeLists.txt](../src/v3/tools/bniutils/CMakeLists.txt)
      registering the five executables (`bnilist`, `bni2tga`,
      `bniextract`, `bnibuild`, `tgainfo`) through a local
      `_pvpgn_v3_add_bniutils_tool` helper. Helper applies
      `pvpgn_v3_apply_flags`, links the legacy `common` + `compat`
      targets, adds `${CMAKE_SOURCE_DIR}/src` to the include path,
      and suppresses strict warnings (`-w`) until each file is
      modernized. Gated on `if(NOT TARGET common) return()`.
- [x] Added `add_subdirectory(tools/bniutils)` to
      [src/v3/CMakeLists.txt](../src/v3/CMakeLists.txt).
- [x] Removed `add_subdirectory(bniutils)` from
      [src/CMakeLists.txt](../src/CMakeLists.txt); consolidated the
      `bnpass` / `bniutils` / `bnproxy` migration notes into a
      single comment block.
- [x] Added the five bniutils targets to
      [Dockerfile.v3](../Dockerfile.v3) v3-build list. Verified
      docker v3-test image now builds all five (`bnilist`,
      `bni2tga`, `bniextract`, `bnibuild`, `tgainfo`); smoke-tested
      `bnilist` with no-arg invocation (prints expected BNI/TGA
      header diagnostic).
- [x] Full docker v3-test suite still green (codec 1225/213, bigint
      36/11, bnetsrp3 1/1, compose 67/14, compose_bridge 33/7,
      compose_e2e 63/6).
- [ ] Future modernization passes (not required for the relocation):
      replace `xalloc` with STL containers, raw stdio with
      `std::filesystem`, drop `setup_before.h` / `setup_after.h`
      wrappers, remove the legacy `common` / `compat` link.

## Side track: `src/v3/tools/bniutils/` modernization

- [x] Added [src/v3/tools/bniutils/xalloc_shim.hpp](../src/v3/tools/bniutils/xalloc_shim.hpp):
      drop-in `pvpgn::xmalloc` / `xrealloc` / `xfree` wrappers
      around `std::malloc` / `std::realloc` / `std::free` with
      abort-on-OOM, matching the long-standing semantics of
      `common/xalloc`.
- [x] Rewrote every include block in `bni.cpp`, `bni2tga.cpp`,
      `bnibuild.cpp`, `bniextract.cpp`, `bnilist.cpp`, `fileio.cpp`,
      `tga.cpp`, `tgainfo.cpp` to drop `common/setup_before.h`,
      `common/setup_after.h`, `common/xalloc.h`, `common/version.h`,
      `compat/statmacros.h`, `compat/mkdir.h`. The local shim is
      pulled in where allocation is needed; `PVPGN_VERSION` is now
      delivered via `target_compile_definitions` with an `"unknown"`
      fallback `#define`.
- [x] Replaced POSIX `stat()` + `S_ISDIR` + `p_mkdir()` in
      [bnibuild.cpp](../src/v3/tools/bniutils/bnibuild.cpp) and
      [bniextract.cpp](../src/v3/tools/bniutils/bniextract.cpp)
      with `std::filesystem::status` / `is_directory` /
      `create_directory`. Removed the `HAVE_SYS_STAT_H` / `sys/stat.h`
      gates.
- [x] [src/v3/tools/bniutils/CMakeLists.txt](../src/v3/tools/bniutils/CMakeLists.txt)
      no longer links legacy `common` / `compat`, no longer adds
      `${CMAKE_SOURCE_DIR}/src` to the include path, no longer gates
      on `if(NOT TARGET common)`. Now passes
      `PVPGN_VERSION="${PVPGN_VERSION}"` as a compile def and
      otherwise relies only on `pvpgn_v3_apply_flags`. Warning
      suppression kept (`-w` / `/w`) for now, as the original BNI/TGA
      logic has several legacy patterns still in flight.
- [x] Docker v3-test build green end-to-end: full Catch2 suite
      passes (1225/213 codec, 36/11 bigint, 67/14 compose_event,
      33/7 compose_bridge, 63/6 compose_e2e, 174/6 protocol_packet,
      etc.). All five binaries (`bnilist`, `bni2tga`, `bniextract`,
      `bnibuild`, `tgainfo`) build with no warnings escaping the
      `-w` envelope.
- [x] Optional next pass: convert raw `t_file` linked-list stack in
      [fileio.cpp](../src/v3/tools/bniutils/fileio.cpp) to
      `std::stack<std::FILE*>` and gradually remove the
      `xalloc_shim` users in favor of `std::vector<std::byte>` /
      `std::unique_ptr<std::FILE, FileCloser>`; once that lands, the
      `-w` warning suppression can be dropped.

### Deep modernization (drop `-w`)

- [x] [fileio.cpp](../src/v3/tools/bniutils/fileio.cpp) now uses
      `std::stack<std::FILE*>` for the push/pop stacks; the legacy
      `t_file` linked list and `xalloc_shim` users are gone.
- [x] All C-style casts converted to `static_cast<>` / local
      `alloc_bytes()` helpers across
      [fileio.cpp](../src/v3/tools/bniutils/fileio.cpp),
      [tga.cpp](../src/v3/tools/bniutils/tga.cpp),
      [bnilist.cpp](../src/v3/tools/bniutils/bnilist.cpp),
      [bni.cpp](../src/v3/tools/bniutils/bni.cpp),
      [bnibuild.cpp](../src/v3/tools/bniutils/bnibuild.cpp),
      [bniextract.cpp](../src/v3/tools/bniutils/bniextract.cpp).
- [x] `xmalloc` / `xrealloc` / `xfree` call sites replaced with
      `std::malloc` / `std::realloc` / `std::free` + abort-on-NULL,
      or with `std::string` / `std::snprintf` idioms in
      `bnibuild.cpp` and `bniextract.cpp`.
- [x] Fixed long-standing tautological-compare bugs in
      [tga.cpp](../src/v3/tools/bniutils/tga.cpp) (`load_tga` &
      `write_tga`): `(img->desc & tgadesc_horz) == 1` -> `!= 0`
      since `tgadesc_horz = 16`. Hidden for years by the `-w`
      envelope.
- [x] `xalloc_shim.hpp` deleted.
- [x] `-w` warning suppression removed from
      [src/v3/tools/bniutils/CMakeLists.txt](../src/v3/tools/bniutils/CMakeLists.txt).
      All five binaries (`bnilist`, `bni2tga`, `bniextract`,
      `bnibuild`, `tgainfo`) now build clean under the full v3
      warning set (`-Wall -Wextra -Wpedantic -Wshadow
      -Wold-style-cast -Wcast-align -Wformat=2`, etc.) with
      `-Werror`. Docker `v3-test` rebuilds green and the full v3
      Catch2 suite still passes (1225/213 codec, 36/11 bigint,
      67/14 chat_event_compose, 174/6 protocol_packet, etc.).

## Side track: `src/bntrackd/` relocation

- [x] Moved `src/bntrackd/{bntrackd.cpp,servers.xsl}` to
      [src/v3/tools/bntrackd/](../src/v3/tools/bntrackd/). Legacy
      `src/bntrackd/` directory deleted.
- [x] Removed `add_subdirectory(bntrackd)` from
      [src/CMakeLists.txt](../src/CMakeLists.txt) and added the
      tool to [src/v3/CMakeLists.txt](../src/v3/CMakeLists.txt)
      alongside `bnpass` / `bniutils`.
- [x] First-pass `src/v3/tools/bntrackd/CMakeLists.txt`: still
      links legacy `common compat fmt ${NETWORK_LIBRARIES}`,
      includes `${CMAKE_SOURCE_DIR}/src` + `${CMAKE_BINARY_DIR}`
      (for `config.h`), and keeps the `-w` envelope. A future
      modernization pass will route through v3 logging /
      networking / packet APIs and let us drop the legacy link
      and `-w` flag, mirroring the bniutils trajectory.
- [x] Added `bntrackd` to the
      [Dockerfile.v3](../Dockerfile.v3) `v3-build` target list.
      Docker `v3-test` builds green and the full Catch2 suite
      still passes (1225/213 codec, 36/11 bigint, 67/14
      chat_event_compose, 174/6 protocol_packet, etc.).
- [x] Deep modernization pass: drop legacy `common`/`compat`,
      replace `t_list serverlist_head` with `std::vector<ServerEntry>`,
      route logging through v3 `core` LOG_* macros, use direct
      POSIX/Winsock2 UDP instead of `compat/psock`, and remove the
      `-w` envelope.

### Deep modernization (drop `-w`) bntrackd

- [x] Full rewrite of
      [bntrackd.cpp](../src/v3/tools/bntrackd/bntrackd.cpp)
      (~600 lines) as a self-contained C++20 daemon. No legacy
      includes (`common/setup_before.h`, `common/eventlog.h`,
      `common/tracker.h`, `common/list.h`, `common/xalloc.h`,
      `common/bn_type.h`, `common/util.h`, `compat/psock.h`,
      `compat/stdfileno.h`, `compat/pgetpid.h` -- all gone).
- [x] `t_list serverlist_head` / `t_elem` / `xmalloc` /
      `list_remove_elem` replaced with `std::vector<ServerEntry>`
      + `std::find_if` + `std::erase_if`.
- [x] `eventlog(eventlog_level_*, __FUNCTION__, "fmt", ...)`
      replaced with `LOG_INFO/LOG_DEBUG/LOG_WARN/LOG_ERROR` from
      [core/format.hpp](../src/v3/core/include/core/format.hpp).
      `--debug` toggles `default_logger().set_level(Debug)`.
- [x] `t_trackpacket` (`bn_short`/`bn_int`/`bn_byte`) replaced
      with a plain `TrackPacket` struct using `std::array<char,N>`
      string slots + `std::uint16_t` / `std::uint32_t`, decoded
      from the wire by hand with `std::memcpy` + `ntohs` /
      `ntohl`. `static_assert(wire size == 464)` guards layout.
- [x] `compat/psock.h` replaced with portable POSIX
      `<sys/socket.h>` / Winsock2 (`#ifdef _WIN32` wraps
      `WSAStartup` + `closesocket`).
- [x] Legacy `str_to_uint` / `str_to_ushort` replaced with
      `std::from_chars`. Argument parsing rewritten using
      `std::string_view::starts_with` + lambdas.
- [x] `--XML` output, `bnetd-servers` ASCII record format, log
      message wording, and CLI flag set kept identical to the
      legacy daemon. Operator tooling and scrapers see the same
      bytes on the wire.
- [x] [CMakeLists.txt](../src/v3/tools/bntrackd/CMakeLists.txt)
      drops `common compat fmt` link and the `${CMAKE_SOURCE_DIR}/src`
      include; now just links `core ${NETWORK_LIBRARIES}` and runs
      under `pvpgn_v3_apply_flags()`. `-w` envelope removed.
- [x] Docker `v3-test` rebuilds green; the binary loads only
      `libstdc++` / `libgcc_s` / `libc` (no legacy `eventlog` /
      `xmalloc` / `list_create` / `psock_*` symbols). Full v3
      Catch2 suite still passes (1225/213 codec, etc.).

## Side track: `src/client/` relocation

- [x] Moved `src/client/{bnchat,bnftp,bnbot,bnstat,client,
      client_connect,udptest,ansi_term}.{cpp,h}` to
      [src/v3/tools/client/](../src/v3/tools/client/). Legacy
      `src/client/` directory deleted.
- [x] Removed `add_subdirectory(client)` from
      [src/CMakeLists.txt](../src/CMakeLists.txt) and added the
      tools to [src/v3/CMakeLists.txt](../src/v3/CMakeLists.txt)
      alongside `bnpass` / `bniutils` / `bntrackd`.
- [x] First-pass `src/v3/tools/client/CMakeLists.txt` defines a
      `_pvpgn_v3_add_client_tool()` helper that links each binary
      against legacy `common compat fmt ${NETWORK_LIBRARIES}`,
      includes `${CMAKE_SOURCE_DIR}/src` + `${CMAKE_BINARY_DIR}`
      (for `config.h`), and keeps the `-w` envelope. A future
      modernization pass will route through v3 logging /
      networking / packet APIs and let us drop the legacy link
      and `-w` flag, mirroring bniutils / bntrackd.
- [x] Added `bnchat bnftp bnbot bnstat` to the
      [Dockerfile.v3](../Dockerfile.v3) `v3-build` target list.
      Docker `v3-test` builds green; all four binaries land in
      `/src/build/v3/src/v3/tools/client/` and the full v3
      Catch2 suite still passes (1225/213 codec, etc.).
- [ ] Deep modernization pass: drop legacy `common`/`compat`,
      route logging through `core` LOG_* macros, replace
      `compat/psock` + `common/network` with portable POSIX /
      Winsock2, retire `xalloc` / `t_list` users, and remove the
      `-w` envelope.

### Deep-modernization scope audit

  The four client binaries collectively depend on the following
  legacy headers (gathered via `grep_search` on
  [src/v3/tools/client/](../src/v3/tools/client/)):

  - `common/packet.h` (~19 KB), `common/bnet_protocol.h` — packet
    framing and the union of every BNet packet body. `t_packet`,
    `packet_create`, `packet_set_size/type`, `packet_get_*`,
    `packet_append_*` are used in all 4 tools plus
    `client_connect.cpp` and `udptest.cpp`.
  - `common/init_protocol.h` — `CLIENT_INITCONN_*`,
    `CLIENT_AUTH_INFO`, `CLIENT_UNKNOWN_1B`, `SERVER_UDPTEST`,
    etc.
  - `common/bn_type.h` — `bn_int`/`bn_short`/`bn_byte` little-endian
    accessors (`bn_int_get/set`, `bn_int_nset`, `bn_short_get/set`,
    `bn_byte_get/set`).
  - `common/bnethash.h` + `bnethashconv.h` — Battle.net broken-SHA1
    password hashing (login path in `client_connect.cpp` only).
  - `common/network.h` — `net_inet_ntop`, hostname resolution
    wrappers.
  - `common/tag.h` — 4cc tag (de)stringification.
  - `common/field_sizes.h` — `MAX_MESSAGE_LEN`,
    `MAX_USERNAME_LEN`, `MAX_PASSWORD_LEN`, etc.
  - `common/util.h` — `str_to_*`, misc helpers.
  - `common/eventlog.h` — `eventlog(eventlog_level_*, ..)` (only
    under `CLIENTDEBUG`).
  - `compat/psock.h` — `psock_socket/recv/send/connect/...` portable
    socket wrappers.
  - `compat/termios.h`, `compat/gethostname.h`.

  Modernization plan (incremental, one tool per round):

  1. **Vendor a minimal `bnclient_proto.hpp`** in
     [src/v3/tools/client/](../src/v3/tools/client/) covering: the
     little-endian `bn_int`/`bn_short`/`bn_byte` POD structs,
     `packet_class_*` enum, the subset of BNet packet structs each
     tool actually instantiates (init-conn, auth-info, ping,
     udp-test, getfile-req, etc.), plus framing helpers
     `blockrecv_packet`/`blocksend_packet` rewritten on raw POSIX
     I/O. Stays free of `common/`.
  2. **Vendor a `bnclient_net.hpp`** with portable socket helpers
     (POSIX/Winsock2) — same pattern as the bntrackd rewrite.
  3. **Vendor `bnclient_hash.hpp`** with the broken-SHA1 used by
     `client_connect.cpp` login path. Cleanroom port from
     `common/bnethash.cpp`.
  4. **Modernize the shared `client.{cpp,h}`, `client_connect.{cpp,h}`,
     `udptest.{cpp,h}`** to consume the vendored headers. Route
     logging through `core/format.hpp` `LOG_*` macros.
  5. **Per tool** (smallest first): `bnbot` -> `bnftp` -> `bnstat`
     -> `bnchat`. Each round: rewrite the tool, rebuild docker,
     verify the binary ldd is clean of legacy symbols, update this
     checklist, ask user before continuing.
  6. **CMake**: switch `_pvpgn_v3_add_client_tool()` to link only
     `core ${NETWORK_LIBRARIES}`, drop the legacy include path,
     remove `-w`, apply `pvpgn_v3_apply_flags()`.

### Deep-modernization round 1: vendored headers + probe

- [x] [src/v3/tools/client/bnclient_net.hpp](../src/v3/tools/client/bnclient_net.hpp)
      — header-only portable socket layer.  POSIX / Winsock2 behind
      `socket_t`, `socklen_portable`, `close_socket`, `last_error`,
      `sockets_startup/cleanup`, `resolve_host`, `connect_tcp`,
      `open_udp_local`, `send_all`, `recv_all`, `addr_to_string`,
      plus a move-only `socket_holder` RAII guard.  Mirrors the
      style used by `src/v3/tools/bntrackd/bntrackd.cpp`.
- [x] [src/v3/tools/client/bnclient_proto.hpp](../src/v3/tools/client/bnclient_proto.hpp)
      — header-only BNet wire primitives.  `bn_byte` / `bn_short` /
      `bn_int` `std::array` PODs with LE accessors (`byte_get`,
      `short_get`, `int_get`) and the network-byte-order twins
      (`short_nget`, `int_nget`, `int_nset`) plus a `int_tag_*`
      4cc helper.  `BnetHeader { type, size }` (4 bytes LE) +
      `kBnetHeaderSize` + `kMaxPacketSize`.  `Packet` fixed-
      capacity buffer with `bnet_type/size`, `set_bnet_type/size`,
      `body_as<T>()`, `append`/`append_cstr`.  Free framing
      functions `send_init_classbyte`, `send_bnet`, `recv_bnet`,
      `recv_raw`.
- [x] [src/v3/tools/client/bnclient_probe.cpp](../src/v3/tools/client/bnclient_probe.cpp)
      + new `bnclient_v3_probe` OBJECT library in
      [src/v3/tools/client/CMakeLists.txt](../src/v3/tools/client/CMakeLists.txt).
      The probe TU exercises every API surface of both vendored
      headers and is compiled under `pvpgn_v3_apply_flags()` (full
      warning set, `-Werror`) so future header drift is caught
      immediately.  Every client tool now `add_dependencies()` on
      it.  Docker `v3-test` builds the probe clean (no
      `error:`/`warning:` lines) and all four tool binaries land
      under `build/v3/src/v3/tools/client/` as before.
- [ ] Round 2 (next): vendor `bnclient_hash.hpp` covering the
      Battle.net broken-SHA1 used by `client_connect.cpp` login.
      Cleanroom port of `common/bnethash.cpp`.

### Deep-modernization round 2: vendored Battle.net hash

- [x] [src/v3/tools/client/bnclient_hash.hpp](../src/v3/tools/client/bnclient_hash.hpp)
      — header-only cleanroom port of the Blizzard-variant
      "broken SHA-1" from
      [src/common/bnethash.cpp](../src/common/bnethash.cpp).
      `pvpgn::client_v3::hash::HashDigest` is
      `std::array<std::uint32_t, 5>`; `bnet_hash(ptr, len)` and
      the template `bnet_hash_object(pod)` return the digest by
      value (no out-parameter). `to_string()` formats as the
      classic 40-char ASCII representation (each host word
      byte-reversed, lowercase hex) and `eq()` is bytewise.
      Standard SHA-1 / little-endian variants are NOT ported --
      none of the four client binaries call them.
- [x] Probe TU
      [src/v3/tools/client/bnclient_probe.cpp](../src/v3/tools/client/bnclient_probe.cpp)
      exercises `bnet_hash`, `bnet_hash_object`, `to_string`, and
      `eq` so any header regression is caught under
      `pvpgn_v3_apply_flags()` (`-Wall -Wextra -Wpedantic -Werror`,
      no `-w` envelope). Docker `v3-test` builds the probe clean.
- [ ] Known-answer validation: defer until the
      `client_connect.cpp` rewrite round. At that point either
      add a transitional Catch2 unit that links *both* the legacy
      `common` lib and the vendored header and asserts bytewise
      digest equality across a corpus of inputs (`""`,
      `"password"`, a 64-byte block, a >64-byte block, a >55-byte
      sub-64 block to exercise the length-padding branch), or
      verify behaviorally by handshaking against a real bnetd.
- [ ] Round 3 (next): modernize the shared TUs
      [src/v3/tools/client/client.cpp](../src/v3/tools/client/client.cpp)
      / [client_connect.cpp](../src/v3/tools/client/client_connect.cpp)
      / [udptest.cpp](../src/v3/tools/client/udptest.cpp) to
      consume the vendored headers; route logging through
      `core/format.hpp` `LOG_*` macros.

### Deep-modernization round 3: bnbot standalone rewrite

- [x] [src/v3/tools/client/bnbot_v3.cpp](../src/v3/tools/client/bnbot_v3.cpp)
      — self-contained v3 rewrite of the legacy
      [bnbot.cpp](../src/v3/tools/client/bnbot.cpp). Connects via
      `pvpgn::client_v3::net::connect_tcp`, sends the
      `CLIENT_INITCONN_CLASS_BOT` (`0x03`) class byte through
      `proto::send_init_classbyte`, then the `^D` bot marker, and
      enters a `select()` loop forwarding raw bytes between
      stdin and the socket. Terminal is configured via a
      RAII `TtyGuard` (POSIX `termios` non-canonical + no-echo;
      `_kbhit`/`_getch` on Windows). Line editing is reduced to
      the BS/DEL/Enter/ESC subset the legacy
      `client_get_comm(visible=-1, redraw=0)` actually exercises
      for this binary.
- [x] [src/v3/tools/client/CMakeLists.txt](../src/v3/tools/client/CMakeLists.txt)
      — the `bnbot` target is removed from
      `_pvpgn_v3_add_client_tool()` and given its own
      `add_executable(bnbot bnbot_v3.cpp)` linking only
      `core ${NETWORK_LIBRARIES}` under `pvpgn_v3_apply_flags()`
      (full warning set + `-Werror`, no `-w` envelope). Legacy
      `bnbot.cpp` is kept in tree for reference but is no longer
      compiled into any target.
- [x] Docker `v3-test` builds `bnbot_v3.cpp` clean. The resulting
      binary is 36 KB and `ldd` shows only `libstdc++` /
      `libgcc_s` / `libc` / `ld-musl`; `nm -D` is empty of any
      legacy `eventlog` / `xalloc` / `list_create` / `psock_*`
      symbol. Full v3 Catch2 suite still passes (1225/213 codec,
      etc.).
- [ ] Pattern proven; remaining rounds repeat per tool:
      `bnftp` -> `bnstat` -> `bnchat`. `bnstat` and `bnchat`
      additionally exercise the BNet login handshake (need
      `bnclient_hash.hpp`) and UDP test (need the udptest port).

### Deep-modernization round 4a: vendored BNet packet bodies

- [x] [src/v3/tools/client/bnclient_bnet_packets.hpp](../src/v3/tools/client/bnclient_bnet_packets.hpp)
      — header-only.  Defines packed POD bodies (no embedded
      4-byte header — the framing layer owns those bytes) for the
      handshake + chat / stats wire surface: `CClientAuthInfo`,
      `SServerAuthReply109`, `SServerAuthReq109`,
      `CClientAuthReq109`, `SServerAuthReq1`, `CClientAuthReq1`,
      `SServerAuthReply1`, `CClientUnknown1B`, `CClientLoginReq1`,
      `SServerLoginReply1`, `CClientStatsReq`, `SServerStatsReply`,
      `CClientEchoReply`, `SServerEchoReq`, `CClientProgIdent`.
      Each carries a wire-size `static_assert`.  Packet-type IDs
      live under `packet_id::` (`CLIENT_AUTH_INFO = 0x50ff`, ...),
      clienttag / archtag / gamelang 4cc literals under `tag::`.
      Result codes (`kAuthReply109_Ok`, `kLoginReply1_Success`,
      ...) and DRTL class enums round out the surface.
- [x] [src/v3/tools/client/bnclient_probe.cpp](../src/v3/tools/client/bnclient_probe.cpp)
      now also instantiates each POD via `Packet::body_as<T>()`,
      fills it through vendored setters, and appends the trailing
      strings to ensure the LE byte-array fields are exercised
      under `pvpgn_v3_apply_flags()` (`-Wall -Wextra -Wpedantic
      -Werror`).  Docker `v3-test` builds the probe clean.
- [ ] Round 4b (next): vendor `bnclient_login.{hpp,cpp}` exposing
      a `LoginSession` driver that runs init -> auth_info -> auth
      challenge -> loginreq1 -> loginreply1 against a connected
      socket, including version-info loading
      (replaces `client_connect.cpp`).
- [ ] Round 4c: write `bnstat_v3.cpp` using `LoginSession`,
      `bnclient_proto::Packet`, and the vendored BNet packet
      bodies; switch the `bnstat` CMake target to
      `core ${NETWORK_LIBRARIES}` only under
      `pvpgn_v3_apply_flags()`.

### Deep-modernization round 4b: vendored BNet login driver

- [x] [src/v3/tools/client/bnclient_login.hpp](../src/v3/tools/client/bnclient_login.hpp)
      — header-only.  Defines:
      * `login::VersionInfo` POD and
        `default_version_info(clienttag)` — cleanroom copy of the
        DRTL/STAR/SSHR/SEXP/W2BN/D2DV/D2XP/WAR3 version constants
        from `client_connect.cpp::get_defversioninfo()`.
      * `login::Config` — server/port + 4cc tags + cdowner/cdkey
        + `ignoreversion` (defaults to `true`, skips
        `CLIENT_AUTHREQ_109`) + `send_cdkey2`
        (STAR/SEXP/W2BN need it).
      * `login::Result` — `sock`, `sessionkey`, `sessionnum`.
      * `login::Session` — `run(Result&)` drives the full
        pre-login handshake on `bnclient_net`/`bnclient_proto`:
        init class byte -> (DRTL/DSHR) `CLIENT_UNKNOWN_1B`
        -> `CLIENT_AUTH_INFO` -> drain to
        `SERVER_AUTHREQ_109`/`AUTHREPLY_109` -> capture
        `sessionkey`/`sessionnum` -> (optional)
        `CLIENT_AUTHREQ_109` + `SERVER_AUTHREPLY_109` -> 
        `CLIENT_ICONREQ` + `SERVER_ICONREPLY` -> (optional)
        `CLIENT_CDKEY2` + `SERVER_CDKEYREPLY2`.  Returns the
        connected socket via `socket_holder::release()`.  No
        dependency on legacy `common/*`.
- [x] Extended `bnclient_bnet_packets.hpp` with the PODs the
      login driver needs: `SServerIconReply`, `CdkeyInfo`,
      `CClientCdkey2`, `SServerCdkeyReply2`, plus
      `packet_id::CLIENT_ICONREQ` / `SERVER_ICONREPLY` /
      `CLIENT_CDKEY2` / `SERVER_CDKEYREPLY2` and the
      `kCdkey2_*` / `kCdkeyReply2_*` constants.
- [x] Probe TU instantiates `login::Config`, `Session`,
      `Result`, calls `default_version_info()`, and takes the
      address of `Session::run` so the full surface compiles
      under `pvpgn_v3_apply_flags()`.  Docker `v3-test` green;
      all 4 client binaries still build.
- [ ] Round 4c (next): write `bnstat_v3.cpp` using `Session`
      + `Packet` to send `CLIENT_LOGINREQ1`, then
      `CLIENT_STATSREQ`, parse `SERVER_STATSREPLY`, and print
      stats; switch `bnstat` CMake target to
      `core ${NETWORK_LIBRARIES}` only under
      `pvpgn_v3_apply_flags()`.

### Deep-modernization round 4c: bnstat standalone

- [x] [src/v3/tools/client/bnstat_v3.cpp](../src/v3/tools/client/bnstat_v3.cpp)
      — modern, self-contained C++20 rewrite of the legacy 1153-LoC
      `bnstat.cpp`.  Argument parser supports
      `-c TAG / --client=TAG`, `-p NAME / --player=NAME`
      (repeatable), `-o OWNER`, `-k CDKEY`,
      `-i / --ignore-version`, `--check-version`, `--bnetd`,
      `--fsgs`, `--stdin`, plus the positional `HOST [PORT]`.
      Defaults: `localhost:6112`, clienttag `STAR`,
      `ignoreversion=true`.
- [x] Wire path:
      `login::Session::run()` -> connected socket positioned just
      after `SERVER_ICONREPLY` -> for each requested player build a
      `CLIENT_STATSREQ` (Packet::body_as<CClientStatsReq>(), append
      player name + per-clienttag key list) -> wait for
      `SERVER_STATSREPLY` -> walk N NUL-terminated value strings
      and print `key = value` lines.
- [x] CMake: `bnstat` switched to `add_executable(bnstat
      bnstat_v3.cpp)`; links only `core ${NETWORK_LIBRARIES}`;
      built under `pvpgn_v3_apply_flags()` (no `-w`, no
      `common/compat/fmt`).  Legacy `bnstat.cpp` kept in tree for
      reference but no longer compiled.
- [x] Docker `v3-test` green; resulting 72 KB binary has no legacy
      symbols (`nm -D | grep -E 'eventlog|psock_|xalloc' == 0`);
      `bnstat -h` prints the new usage message.
- [ ] Round 4d (next): vendor `bnstat_v3.cpp` no longer prints the
      bespoke per-field labels from the legacy tool.  If the
      cosmetic output matters, layer a key-aware formatter on top
      (e.g. parse-time conversion of `lastlogin_time` from epoch
      to `ctime`, gold display, etc.).  Otherwise move on to
      `bnftp_v3.cpp` / `bnchat_v3.cpp`.

### Deep-modernization round 5: bnftp standalone

- [x] [src/v3/tools/client/bnftp_v3.cpp](../src/v3/tools/client/bnftp_v3.cpp)
      -- modern, self-contained C++20 rewrite of the legacy
      ~800-LoC `bnftp.cpp`.  Connects via TCP, sends the
      FILE-class init octet (`0x02`), issues a
      `CLIENT_FILE_REQ` packet, parses `SERVER_FILE_REPLY`,
      and streams the raw file body to disk in 4 KiB chunks.
      No BNet handshake, no auth, no login.
- [x] FILE-class framing is inlined (`FileHeader { size, type }`
      -- note: size precedes type, OPPOSITE of the BNet header
      order).  `CClientFileReq` + `SServerFileReply` PODs are
      `#pragma pack(push,1)` and `static_assert`-sized.
- [x] CLI: `-f FILE / --file=FILE` (required),
      `-c TAG / --client=TAG`, `-a TAG / --arch=TAG`,
      `--startoffset=N`, `--exists=O|B|R` (Overwrite/Backup/
      Resume), positional `HOST [PORT]`.  No interactive prompts.
      `--war3` exits with EXIT_FAILURE -- the three-step
      `FILE_REQ2 / unknown / FILE_REQ3` exchange is not yet
      ported.
- [x] CMake: `bnftp` switched to `add_executable(bnftp
      bnftp_v3.cpp)`; links only `core `;
      built under `pvpgn_v3_apply_flags()` (full warning set
      + `-Werror`, no `-w`).  Legacy `bnftp.cpp` /
      `client.cpp` kept in tree for reference but not compiled.
- [x] Docker `v3-test` green; resulting binary has no legacy
      symbols (`nm -D | grep -E 'eventlog|psock_|xalloc' == 0`);
      `bnftp -h` prints the new usage message.
- [ ] Round 6 (next): `bnchat_v3.cpp` standalone -- largest
      tool (~1700 LoC), needs chat-event parsing + an
      ansi-terminal surface (or a polished pretty printer for
      `bnstat` first).

### Deep-modernization round 5b: bnftp W3 3-step protocol

- [x] Added `CClientFileReq2` (16-byte body, FILE-class
      type 0x0200) and `CClientFileReq3` (52-byte body, RAW --
      no FILE header) PODs to bnftp_v3.cpp, with
      `static_assert`-checked sizes.
- [x] WAR3/W3XP client tag auto-enables `--war3`.  Flow:
      send init class byte FILE -> send REQ2 -> drain server's
      4-byte unknown reply (file-class packet) -> send REQ3
      raw (no header) + filename -> recv SERVER_FILE_REPLY ->
      stream filelen raw bytes to disk.  Legacy single-step path
      is unchanged for non-W3 clients.
- [x] Docker `v3-test` green; `bnftp -h` usage now documents
      the W3 protocol auto-enable.

### Deep-modernization round 6: bnchat standalone

- [x] [src/v3/tools/client/bnchat_v3.cpp](../src/v3/tools/client/bnchat_v3.cpp)
      -- modern, self-contained C++20 rewrite of the legacy
      ~1968-LoC `bnchat.cpp` + `client.cpp` +
      `client_connect.cpp` + `udptest.cpp`.  Flow:
      `bnclient_login::Session::run()` (BNet handshake) ->
      inline `CLIENT_LOGINREQ1` (double-SHA1 password ladder
      via `bnclient_hash::bnet_hash`) ->
      `SERVER_LOGINREPLY1` success check ->
      `CLIENT_PROGIDENT2` -> drain `SERVER_CHANNELLIST` ->
      `CLIENT_JOINCHANNEL` ->
      `select()`-driven chat loop reading line-buffered stdin
      (`CLIENT_MESSAGE` out) and dispatching incoming
      `SERVER_MESSAGE` packets to a renderer that labels by
      type (TALK/JOIN/PART/WHISPER/...).
- [x] Extended [bnclient_bnet_packets.hpp](../src/v3/tools/client/bnclient_bnet_packets.hpp)
      with chat-class packet ids (`CLIENT_PROGIDENT2`,
      `SERVER_CHANNELLIST`, `CLIENT_JOINCHANNEL`,
      `CLIENT_MESSAGE`, `SERVER_MESSAGE`) and matching PODs
      (`CClientProgIdent2`, `CClientJoinChannel`,
      `SServerMessage`) plus `kJoinChannel_*` and
      `kMsgType_*` constants.
- [x] CLI: `-u USER` (required), `-p PASS` (required),
      `-c TAG / --client=TAG`, `--channel=NAME`,
      `-k CDKEY`, `-o OWNER`, positional `HOST [PORT]`.
      Type `/quit` to exit; stdin EOF also exits cleanly.
- [x] Deliberately drops legacy interactive features:
      character-at-a-time line editor (`client_get_comm`),
      ansi-terminal coloring, UDP test (`udptest.cpp` /
      `CLIENT_NETINFO`), account creation
      (`CLIENT_CREATEACCTREQ1`), Diablo 1 PLAYERINFOREQ stat
      upload.  Those are valuable but tangential to the
      modernization objective.
- [x] CMake: `bnchat` switched to `add_executable(bnchat
      bnchat_v3.cpp)`; links only `core `;
      built under `pvpgn_v3_apply_flags()`.  Legacy
      `bnchat.cpp` + `client.cpp/h` + `client_connect.cpp`
      + `udptest.cpp` + `ansi_term.h` are kept in tree as
      reference but no longer compiled.
- [x] Docker `v3-test` green; 55 KB binary, no legacy symbols
      (`nm -D | grep -E 'eventlog|psock_|xalloc|packet_create'
      == 0`); `bnchat -h` prints the new usage.
- [x] All four client binaries (bnbot/bnftp/bnstat/bnchat) now
      compile cleanly under `-Wall -Wextra -Wpedantic -Werror`
      and depend only on `core` + ``.
      The legacy `_pvpgn_v3_add_client_tool()` helper is no
      longer used by any tool (kept in CMakeLists.txt for future
      reuse if needed).
- [ ] Optional follow-ups: account-creation flow, UDP NETINFO
      latency test, ansi-color renderer, character-at-a-time
      line editing, Diablo 1 stat upload.

## Deep-modernization round 7: bnftp e2e docker smoke test

- New file `tests/e2e/fake_bnftp_server.py` -- ~140-line Python mock
  of the FILE-class BNFTP server. Accepts one connection, validates
  init class byte 0x02, decodes CLIENT_FILE_REQ (type 0x0100), and
  serves a fixed payload (`hello from fake bnftp server\n`) inside a
  SERVER_FILE_REPLY (type 0x0000) followed by raw bytes.
- New file `scripts/v3-e2e-bnftp-smoke.sh` -- POSIX shell harness.
  Starts the mock in background, sleeps 0.3s, runs the v3 `bnftp`
  against 127.0.0.1, `wait`s the mock, and compares the downloaded
  bytes to the expected payload. Both client and mock logs are dumped
  on failure.
- `Dockerfile.v3` -- new `v3-e2e` stage that `apk add python3`
  and runs the harness. Verified green:
  `[v3-e2e-bnftp] OK: payload matches (29 bytes)`.
- Pivoted away from "spin up real bnetd in the test container":
  bnetd needs full config trees and an account DB. Wire-level mocks
  are tractable per protocol, so the same pattern can be extended to
  bnchat/bnstat later if desired.
- Gotcha: an `nc -z` readiness probe will eat the mock's single
  accept() and starve the real client. A short `sleep` is enough.
- Gotcha: bind/connect on `127.0.0.1` explicitly -- `localhost`
  may resolve to `::1` inside the container while the mock only
  binds v4.


## Deep-modernization round 8: cleanup legacy client sources

- Deleted `src/v3/tools/client/{bnchat,bnftp,bnstat,bnbot}.cpp`,
  `client.{cpp,h}`, `client_connect.{cpp,h}`,
  `udptest.{cpp,h}`, and `ansi_term.h`. All four binaries now
  build solely from their `<name>_v3.cpp` rewrites plus the shared
  `bnclient_*.hpp` headers.
- Rewrote `src/v3/tools/client/CMakeLists.txt`: dropped the unused
  `_pvpgn_v3_add_client_tool` helper (legacy `common`/`compat`
  link, `-w` envelope) and shortened the preamble. Each tool is
  now a one-line `add_executable` + `pvpgn_v3_apply_flags` +
  install rule.
- Verified green: `docker build --target v3-e2e` still reports
  `[v3-e2e-bnftp] OK: payload matches (29 bytes)`.


## Deep-modernization round 9: bnchat e2e docker smoke test

- New file `tests/e2e/fake_bnet_server.py` -- parameterized Python
  mock of the bnetd chat surface. Walks the BNet handshake
  (CLIENT_AUTH_INFO -> SERVER_AUTHREQ_109 -> CLIENT_ICONREQ ->
  SERVER_ICONREPLY -> CLIENT_LOGINREQ1 -> SERVER_LOGINREPLY1=ok ->
  CLIENT_PROGIDENT2 -> SERVER_CHANNELLIST -> CLIENT_JOINCHANNEL),
  then pushes one SERVER_MESSAGE (type=5/Talk, name=mockuser,
  text="hello from fake bnetd") and closes. Validates packet types
  only, not bodies -- right granularity for a wire smoke.
- New file `scripts/v3-e2e-bnchat-smoke.sh` -- starts the mock,
  pipes `(sleep 1; printf '/quit\\n')` into `bnchat` to keep
  stdin alive long enough for the SERVER_MESSAGE to be rendered,
  then `grep -F`s the expected substring in stdout.
- `Dockerfile.v3` v3-e2e stage now runs both bnftp and bnchat
  smokes. Verified green: `[v3-e2e-bnchat] OK: SERVER_MESSAGE
  rendered`.
- Gotcha: `bnchat`'s `--channel` long option requires the
  `--channel=NAME` form (no space-separated variant).
- Gotcha: with stdin = `/dev/null`, `bnchat` exits immediately
  on EOF and races the server message. A short `sleep` in the
  stdin generator avoids that without making the test flaky.


## Deep-modernization round 10: bnstat e2e docker smoke test

- `tests/e2e/fake_bnet_server.py` -- added a `--mode {chat|stats}`
  switch.  `stats` mode shares the handshake path
  (AUTH_INFO/AUTHREQ_109/ICONREQ/ICONREPLY), then expects one
  CLIENT_STATSREQ and answers with a SERVER_STATSREPLY where each
  value cycles through `["male","42","moon","fake account"]`.
- New file `scripts/v3-e2e-bnstat-smoke.sh` -- drives bnstat
  against the mock with `-c D2DV -p smoketest` (D2DV skips the
  CDKEY2 stage that the mock does not model) and `grep -F`s
  `moon` out of bnstat's stdout.
- `Dockerfile.v3` v3-e2e stage now runs three smokes:
  `[v3-e2e-bnftp] OK ... [v3-e2e-bnchat] OK ... [v3-e2e-bnstat] OK`.
- Gotcha: `set -e` + `if ! cmd; then rc=\True; ...` is a footgun --
  inside the then-branch, `\True` is the value AFTER the `!`
  inversion (0), so the script exits 0 even when the command
  failed.  Use `set +e; cmd; rc=\True; set -e; if [ \"\\" -ne 0 ]`
  instead.
- Gotcha: bnstat's clienttag whitelist is STAR / SEXP / SSHR / DRTL /
  DSHR / W2BN / D2DV / D2XP / WAR3 -- no CHAT.  Use D2DV or W3-class
  to skip CDKEY2 without the DRTL-only UNKNOWN_1B prelude.


## Deep-modernization round 11: extract shared mock helper

- New file `tests/e2e/fake_bnet_common.py` -- stdlib-only helper
  with `recv_exact` / `recv_bnet` / `send_bnet` / `expect` /
  `listen_one` / `do_handshake`.  `do_handshake` parameterizes
  the init-class byte so a future BOT-class mock can reuse it
  (default 0x01 for BNet).
- Rewrote `fake_bnftp_server.py` and `fake_bnet_server.py` to
  `from fake_bnet_common import ...`.  Each file is now ~100 LoC
  smaller and only contains the bits that are actually different
  between the FILE-class single-shot transfer and the two BNet
  conversation flows (chat / stats).
- v3-e2e docker stage still green:
  `[v3-e2e-bnftp] OK ... [v3-e2e-bnchat] OK ... [v3-e2e-bnstat] OK`.
- `listen_one` returns the accepted socket as a context manager
  (sockets implement `__enter__/__exit__` since 3.5) so callers
  can `with listen_one(...) as conn:` without bookkeeping.


## Deep-modernization round 12: bnbot e2e docker smoke test

- New file `tests/e2e/fake_bnbot_server.py` -- BOT-class mock that
  validates the `0x03` init class byte and the `\\x04\\x00` bot
  marker, then writes `"Welcome from fake bnbot\\r\\n"` and closes.
- New file `scripts/v3-e2e-bnbot-smoke.sh` -- bnbot does not exit
  on its own (it spins on stdin EOF), so the script runs it
  backgrounded with `</dev/null`, sleeps 1.5s, kills it, and
  `grep -F`s the banner out of stdout.
- `Dockerfile.v3` v3-e2e stage now runs all four smokes:
  bnftp / bnchat / bnstat / bnbot.  All green.
- All four v3 client binaries now have end-to-end coverage against
  wire-level Python mocks.  The legacy bnetd is never spun up in
  the test container -- the strategy is permanent.


## Side track: `src/win32/dirent.h` retirement

The last surviving piece of the legacy-tools plan `Step 5c` --
replace the bundled MSVC dirent shim with `<filesystem>`.

- Audited every file in `src/win32/`; the directory is the entry
  glue for the three legacy server binaries and contains no dead
  code.  Resource files (`*_resource.rc`) and entry points
  (`winmain.cpp`, `d2cs_winmain.cpp`, `d2dbs_winmain.cpp`)
  are all referenced under `WIN32_GUI` / `WIN32` guards.
- Rewrote `pvpgn::dir_getfiles` and `pvpgn::is_directory` in
  `src/compat/pdir.cpp` to use `std::filesystem::directory_iterator`.
  The `Directory` class itself already used `_findfirst` /
  `_findnext` on Windows -- the dirent shim was only needed by
  these two free functions.
- Dropped `src/win32/dirent.h` from `src/win32/CMakeLists.txt`
  and deleted the file.
- Bumped the `compat` static library to C++17 via
  `target_compile_features(compat PUBLIC cxx_std_17)`; the rest of
  the legacy tree stays C++11.  Added a GCC < 9 fallback that links
  `stdc++fs` for the older toolchains that need it.
- Verified: `docker build -f Dockerfile --target build-plain` now
  reaches `Built target bnetd` / `d2cs` / `d2dbs`.  v3 e2e
  smokes still green: bnftp / bnchat / bnstat / bnbot all OK.


## Side track: compat filesystem cluster retirement

- Deleted shims: `src/compat/access.h` and `src/compat/statmacros.h`.
- Rewrote `src/compat/mkdir.h` as a thin inline `std::filesystem::create_directory` wrapper; mode arg removed from API and all call sites.
- Rewrote `src/compat/rename.h` as a thin inline `std::filesystem::rename` wrapper.
- `pdir.cpp` free functions (`dir_getfiles`, `is_directory`) ported to `std::filesystem::directory_iterator` (Directory class POSIX branch retained for now).
- Patched call sites: bnetd/support.cpp, bnetd/storage_file.cpp, bnetd/mail.cpp, bnetd/userlog.cpp, d2cs/d2charfile.cpp, d2cs/handle_d2cs.cpp (3 p_mkdir calls), d2dbs/dbspacket.cpp (stat+access gates and p_mkdir).
- compat lib carries `target_compile_features(compat PUBLIC cxx_std_17)` which propagates transitively.
- Verified: legacy docker build (compat/bnetd/d2cs/d2dbs all Built) and v3 e2e (bnftp/bnchat/bnstat/bnbot all OK).


## Side track: compat/strdup retirement

- Deleted `src/compat/strdup.h` and `src/compat/strdup.cpp` (HAVE_STRDUP fallback was dead on every supported toolchain).
- Dropped stale `#include `compat/strdup.h`` from bnetd/account.cpp, bnetd/channel.cpp, bnetd/anongame.cpp, bnetd/storage.cpp, common/xstring.cpp (none of them call strdup).
- In common/xalloc.cpp the include was swapped to `<cstring>` (only true caller of unqualified strdup via xstrdup_real).
- Removed strdup.h and strdup.cpp from COMPAT_SOURCES in src/compat/CMakeLists.txt.
- Verified: legacy docker build (compat/bnetd/d2cs/d2dbs all Built) and v3 e2e (all four OK).


## Side track: compat/strcasecmp + compat/strncasecmp retirement

- Deleted `src/compat/strcasecmp.{h,cpp}` and `src/compat/strncasecmp.{h,cpp}`.
- Removed them from COMPAT_SOURCES in src/compat/CMakeLists.txt.
- Global sweep across 51 call sites: `#include `compat/strcasecmp.h`` and `#include `compat/strncasecmp.h`` rewritten to portable POSIX `#include <strings.h>`; adjacent duplicates collapsed in 7 files; 8 files received an added `#include <cstring>` to keep `std::strXXX` visible after losing the transitive include.
- Verified: legacy docker build (compat/bnetd/d2cs/d2dbs all Built) and v3 e2e (all four OK).
- Note: `<strings.h>` is POSIX-only; if a Windows build path is revived, a single mapping (`_stricmp`/`_strnicmp`) will need to be added to setup_before.h.


## Side track: bniutils modernization (stateless fileio + RAII t_bnifile)

- `fileio.{h,cpp}` rewritten: dropped the global `std::stack<std::FILE*>` plus the `file_rpush`/`rpop`/`wpush`/`wpop` API. Every reader/writer now takes `std::FILE*` as its first argument.
- `bni.h`: removed `BNI_MAXICONS` cap, removed `bni_iconlist_struct` middle hop, and replaced the fixed-size icon array with `std::vector<t_bniicon> icons`. Added `destroy_bni` for callers that want explicit cleanup.
- `bni.cpp`: `load_bni` now `new t_bnifile{}` + `icons.resize(numicons)`; `write_bni` and `load_bni` thread `f` through every read/write.
- Swept call sites with a regex pass (PowerShell): tga.cpp/tgainfo.cpp/bni2tga.cpp/bnilist.cpp all dropped push/pop and gained explicit FILE* args. `load_tgaheader` signature changed from `()` to `(std::FILE *f)`; updated tgainfo, bnilist, and tga internal load_tga caller.
- `bnibuild.cpp::read_list` replaced its manual `malloc/realloc` on `bni_iconlist_struct` with `icons.push_back(t_bniicon{...})`.
- `bniextract.cpp`/`bni2tga.cpp`: rewrote `bni->icons->icon[i]` to `bni->icons[i]` (vector indexing).
- Verified: v3 docker build green; all 5 bniutils tools (bnilist/bni2tga/bniextract/bnibuild/tgainfo) compile and link; v3 e2e smokes (bnftp/bnchat/bnstat/bnbot) still OK.


## Side track: bniutils malloc+abort retirement (RAII vector data)

- t_tgaimg::data: `std::uint8_t*` -> `std::vector<std::uint8_t>`. Header pulls in `<vector>`.
- new_tgaimg / load_tgaheader: `std::malloc(sizeof(t_tgaimg))` + abort -> `new t_tgaimg{}` (default-constructs vector empty; throws std::bad_alloc on OOM).
- destroy_img: now a single `delete img;` (vector destructor frees data).
- load_tga error paths: `std::free(img)` / `std::free(img->data)` cleanups -> `delete img;`.
- rotate_updown / rotate_leftright / RLE_compress: `alloc_bytes` + manual free -> stack-local `std::vector<std::uint8_t>` scratch buffer; final assignment via `std::move`.
- Removed unused `alloc_bytes` helper from tga.cpp anonymous namespace.
- bniextract::cutimg + bnibuild main: `dst->data = malloc(...)` + abort -> `dst->data.resize(...)`; raw pointer arithmetic now uses `.data()`.
- CMakeLists.txt comment block: dropped stale `xalloc_shim.hpp` reference; documents RAII / std::bad_alloc behavior.
- v3 docker e2e: all 5 bniutils targets compile; 4 e2e smokes still `OK:` (bnftp, bnchat, bnstat, bnbot).

## Side track: bniutils fileio rewired onto core::endian

- src/v3/tools/bniutils/CMakeLists.txt: tools now link the v3 `core` library (header propagation for `core/bytes.hpp` / `core/endian.hpp`).
- src/v3/tools/bniutils/fileio.cpp: hand-rolled shift+OR sequences for u8/u16/u32 read/write LE/BE deleted. Replaced with templated `read_le_from_file<T>` / `read_be_from_file<T>` / `write_le_to_file<T>` / `write_be_to_file<T>` helpers backed by `core::read_le<T>` / `core::write_le<T>` on a `std::array<std::byte, sizeof(T)>` scratch buffer.
- CMakeLists.txt header comment updated to describe the new core dependency.

## Side track: compat/strsep retirement

- 3 consumers: src/d2dbs/dbspacket.cpp, src/bnetd/game_conv.cpp, src/bnetd/ipban.cpp.
- dbspacket.cpp: rewrote `dbs_verify_ipaddr` to walk a `std::string_view` with `find(',')` (drops the xstrdup+strsep+xfree pair and the `adlist` buffer).
- game_conv.cpp: introduced a file-local `bn_strsep` helper (anonymous namespace, ~6 lines using `std::strcspn`) and swept all 9 `strsep(` -> `bn_strsep(` via regex.
- ipban.cpp: the single `strsep` site inlined directly into the loop body (no helper needed).
- Deleted src/compat/strsep.{h,cpp}; removed from src/compat/CMakeLists.txt.

## Side track: compat/gettimeofday retirement (std::chrono)

- src/common/bnettime.cpp: `bnettime()` now uses `std::chrono::system_clock::now()` decomposed into seconds + microseconds via `duration_cast`.
- src/bnetd/tick.cpp: `get_ticks()` simplified to a `static` chrono baseline (one second before first call) and `duration_cast<milliseconds>` of the delta - no errno path needed.
- Deleted src/compat/gettimeofday.{h,cpp}; removed from src/compat/CMakeLists.txt.

## Side track: compat/uname retirement (direct <sys/utsname.h>)

- src/bnetd/main.cpp: replaced `compat/uname.h` with `HAVE_SYS_UTSNAME_H`-gated direct include; wrapped the print block in `#ifdef HAVE_UNAME` (Win32 build skips the line silently).
- src/bnetd/tracker.cpp: same swap; wrapped the entire utsbuf/snprintf block in `#ifdef HAVE_UNAME` with a `memset` fallback on the else branch.
- Deleted src/compat/uname.{h,cpp}; removed from src/compat/CMakeLists.txt.

## Side track: compat/mmap retirement (dead code)

- `pmmap` / `pmunmap` had no callers anywhere in the tree (only definitions and the macro forwarders in compat itself).
- Deleted src/compat/mmap.{h,cpp}; removed from src/compat/CMakeLists.txt.

## Side track: NOT retired this round

- compat/strerror: kept. Its Win32 branch decodes the full WSA error table (WSAEINTR / WSAEACCES / WSAENETUNREACH / ...) into human-readable strings, which std::strerror does not. Removing would silently regress error messages on the Windows build.

## Verification

- Legacy docker build (compat + bnetd + d2cs + d2dbs on Alpine GCC): all 4 targets green.
- v3 docker build (Dockerfile.v3 target v3-e2e): all 5 bniutils tools rebuilt; 4 e2e smokes still OK (bnftp, bnchat, bnstat, bnbot).

## Side track: compat survey documented (pgetopt + psock kept)

- Wrote docs/compat-survey.md cataloguing the remaining 14 compat shim files: which were retired this session, which stay, and why the two big ones (pgetopt, psock) are intentionally left alone.
- pgetopt: 27 KB of vendored `getopt_long`, but the entire body is gated by `#ifndef HAVE_GETOPT`. On Alpine/glibc/musl/BSD the .cpp compiles to nothing - no callers are paying for it. Useful only on Win32. Leave.
- psock + socket/read/recv/send: hides four real POSIX vs Win32 differences (closesocket, fcntl/ioctl FIONBIO, errno/WSAGetLastError, SOCKET vs int). Replacing requires Boost.Asio or netts, touches every connection loop in bnetd/d2cs/d2dbs. Belongs in its own networking refactor plan, not this side-track.


## Side track: t_connection per-field migration round 1 (clientexe, clientver, loggeduser)

Per user direction "Change one t_connection field at a time ... Aim for 3-5 fields per round". Round 1: 3 fields.

Changes:
- src/bnetd/connection.h: added `#include <string>`; `char const * clientver` -> `std::string clientver`, `char const * clientexe` -> `std::string clientexe`, `const char * loggeduser` -> `std::string loggeduser`.
- src/bnetd/connection.cpp:
  - `xmalloc(sizeof(t_connection))` -> `new t_connection{}` so std::string members get default-constructed.
  - `xfree(c)` -> `delete c` to invoke destructor (frees the new std::string members).
  - Dropped three NULL initializers (`clientver = NULL`, `clientexe = NULL`, `loggeduser = NULL`) in `conn_create`.
  - Dropped three xfree calls (`clientver`, `clientexe`, `loggeduser`) in conn_destroy.
  - `conn_set_clientexe` / `conn_set_clientver`: removed xstrdup+xfree temp pattern, direct std::string assignment.
  - `conn_get_clientexe` / `conn_get_clientver`: `.empty()` check + `.c_str()` return; public API still `char const *`.
  - `conn_set_loggeduser`: kept branching logic incl. the latent `std::string("#" + userid)` bug (preserved per minimal-change rule); replaced `xstrdup`/xfree with std::string assignment; `std::move(temp)` into struct.
  - `conn_get_loggeduser`: `.empty()` instead of NULL, `.c_str()` return.
  - The mid-handshake reset block (around line 1463) now uses `.empty()` / `.clear()` for clientexe instead of xfree+NULL.
- All callers use getters/setters; only two log-string literals in handle_wol.cpp/irc.cpp reference `loggeduser` and remain valid as plain text.

Validation: legacy docker build (compat, common, bnetd_legacy, bnetd, d2cs, d2dbs) green; v3 docker build (5 bniutils + 4 e2e smokes OK: bnftp, bnchat, bnstat, bnbot) green.

Remaining char-const-star fields in t_connection that follow the same xstrdup/xfree pattern (candidates for round 2):
- struct client: country, host, user, owner, cdkey.
- struct chat: tmpOP_channel, tmpVOICE_channel, away, dnd, lastsender, irc.ircline, irc.ircpass.
- struct d2: realminfo, charname.
- struct w3: w3_playerinfo, client_proof, server_proof.
- struct wol: apgar (and a few more).


## Side track: blanket xmalloc/xfree audit + 4 leaf modernizations

Per user request "find all malloc or xmalloc and modernize them" -- the codebase has 500+ matches across src/, well beyond a single round. Catalogued the full inventory in `docs/alloc-survey.md` (categories 1-5 by risk/scope). This round picks four small, self-contained leaf cleanups:

- src/d2cs/handle_signal.cpp: handle_signal()'s loglevels strtok loop now uses `std::string temp(levels); std::strtok(temp.data(), ",");` instead of `xstrdup(...) ... xfree(temp)`.
- src/d2cs/main.cpp: config_init()'s loglevels strtok loop migrated the same way. Also added `#include <string>`.
- src/d2cs/s2s.cpp: s2s_create()'s tserver buffer migrated to std::string with `find(':')` / `resize()` instead of `strchr` + writing '\0' into an xstrdup'd buffer.
- src/bnetd/anongame_wol.cpp: anongame_wol_tokenize_line()'s `line = xmalloc(strlen(text)+2)` working buffer migrated to `std::vector<char>`; dropped the xfree on every return path.

Validation: legacy docker build (compat, common, bnetd_legacy, bnetd, d2cs, d2dbs) green.

Sites still pending (full inventory in docs/alloc-survey.md):
- Category 1: 11 nearly identical path-building blocks in src/d2cs/d2charfile.cpp + 3 in handle_d2cs.cpp + 1 in d2ladder.cpp.
- Category 2: pidfile path in src/d2cs/main.cpp (signature change required); alias and attrgroup helpers.
- Category 3: ~15 struct xmalloc/xfree pairs (anongame, attrgroup, d2cs::connection, d2gs, d2charlist, d2ladder, handle_d2cs, serverqueue).
- Category 4: maplist_* globals in anongame_maplists.cpp; base64/hex buffers in account_wrap.cpp.
- Category 5: anongame_infos.cpp lifetime-of-daemon globals (deferred).

## Round 3: sweep all xmalloc/xfree/xstrdup in d2cs + bnetd

Eliminated remaining xalloc usage across the bulk of d2cs and 8 bnetd files.
All sites converted to std::vector<char>/std::string for transient buffers,
new T{}/delete or new T[]{}/delete[] for owning struct/array allocations.

Files changed (12):
- src/d2cs/d2charfile.cpp - 11 path string sites -> std::vector<char>.
- src/d2cs/handle_d2cs.cpp - 4 path strings, motd xstrdup, charinfo/ccharlist
  struct lifecycles. Added <string> + <vector>.
- src/d2cs/d2charlist.cpp - t_d2charlist xmalloc -> new.
- src/d2cs/d2ladder.cpp - ladderfile/ladderheader/ladderinfo transient
  vectors; ladder_data + ladder_data[].info global arrays via new[]/delete[].
- src/d2cs/d2gs.cpp - t_d2gs xmalloc -> new.
- src/d2cs/serverqueue.cpp - t_sq xmalloc -> new.
- src/bnetd/account_wrap.cpp - temp_buffer xmalloc -> std::vector<char>.
  Skipped account_get_rawattr (existing caller-leak; needs API change).
- src/bnetd/anongame_gameresult.cpp - gameresult/players/heroes -> new[]/delete[].
- src/bnetd/anongame_wol.cpp - t_anongame_wol_player -> new/delete.
- src/bnetd/anongame.cpp - t_matchdata, t_saf_pt2, t_anongameinfo -> new/delete.
- src/bnetd/alias_command.cpp - replace_args growable buffer + offsets array +
  alias/output struct + xstrdup'd strings -> new[]/delete[] (with const_cast
  on delete for char const* members).
- src/bnetd/anongame_maplists.cpp - 4 maplist xstrdup'd globals -> new char[].
- src/bnetd/attrgroup.cpp - t_attrgroup + escape_key tmp + key_get_tab return
  -> new[]/delete[] with const_cast on free sites.

Validation:
- Legacy docker build: green (fmt, common, compat, win32, bnetd_legacy,
  bnetd, d2cs, d2dbs all built).
- v3 docker build + e2e: green (all 5 bniutils, 4 e2e smokes OK).

Deferred (still using xmalloc/xfree on purpose):
- account_get_rawattr in account_wrap.cpp - existing memory leak; callers
  treat return as borrowed; requires owning-type API change.
- Other category-5 lifetime-of-daemon globals catalogued in
  docs/alloc-survey.md.
## Round 3 extension: common/ foundational sweep

Additional 3 common/ files modernized after Round 3:
- src/common/list.cpp - t_list and t_elem allocs -> new/delete.
- src/common/queue.cpp - t_queue alloc -> new/delete; ring buffer xrealloc
  -> new t_packet*[]/memcpy/delete[] grow pattern.
- src/common/tag.cpp - tag_check_in_list xstrdup'd working copy -> std::string;
  refactored away the two goto labels.

Validation:
- Legacy docker build: green.

Remaining sites in src/ (per latest survey):
  ~870 xalloc references across ~70 files. High-count files:
  - bnetd/message.cpp (75), connection.cpp (72), game.cpp (64),
    storage_file.cpp (56), ipban.cpp (51), handle_apireg.cpp (47),
    anongame_infos.cpp (40, category 5 - lifetime-of-daemon), icons.cpp (34),
    sql_dbcreator.cpp (34), channel.cpp (32), tournament.cpp (28).
  - common/addr.cpp (25), bigint.cpp (24, needs vector<bigint_base> refactor),
    trans.cpp (15), hashtable.cpp (8), bnetsrp3.cpp (7), util.cpp (6).
  - d2dbs/d2ladder.cpp (24), d2cs/connection.cpp (15), d2cs/game.cpp (14).
## Round 4: complete d2cs + d2dbs sweep

Per user request, finished all remaining xalloc sites in d2cs and d2dbs.

d2cs files swept this round:
- src/d2cs/connection.cpp - t_connection + char* fields (account, charname)
  + t_d2charinfo_summary -> new/delete with const_cast on delete[].
- src/d2cs/game.cpp - t_game + t_game_charinfo + xstrdup'd name/pass/desc/
  charname -> new/delete with const_cast on delete[].
- src/d2cs/gamequeue.cpp - t_gq -> new/delete.
- src/d2cs/main.cpp - pidfile xstrdup -> new char[]/delete[].

d2dbs files swept this round:
- src/d2dbs/d2ladder.cpp - 24 sites: ladder_file/backup_file (lifetime globals),
  t_d2ladder struct, ladderindex/ladderinfo arrays, info per-type array, raw
  ladder file buffers - all moved to new T[]/delete[] or new T{}/delete.
- src/d2dbs/main.cpp - pidfile xstrdup + loglevels std::strtok working copy
  -> new char[]/delete[] and std::string respectively.
- src/d2dbs/handle_signal.cpp - loglevels std::strtok working copy
  -> std::string.
- src/d2dbs/charlock.cpp - clitbl/gsqtbl arrays + t_charlockinfo entries
  -> new[]/delete[] and new/delete.
- src/d2dbs/dbserver.cpp - t_d2dbs_connection + t_preset_d2gsid -> new/delete.

Remaining intentionally-skipped in d2cs/d2dbs:
- src/d2cs/cmdline.cpp + src/d2dbs/cmdline.cpp: one xfree each, paired with
  common/conf.cpp's internal xstrdup in conf_set_str. Conversion requires
  changing the conf_ API or its internals; deferred.

Validation:
- Legacy docker build: green (all 8 targets built).

Status:
- d2cs/d2dbs subtrees effectively complete (only conf-API-coupled sites
  remain).
- common/ partially done (list, queue, tag); foundational refactors
  (bigint vector, hashtable key ownership, addr/trans/util) still pending.
- bnetd/ partially done (7 files from Round 3); high-count files
  (message, connection, game, storage_*, ipban, channel, tournament, etc.)
  still pending.
## Round 5: common/ sweep

Per user request, swept remaining common/ files (excluding bigint and
bnetsrp3 which require coordinated refactor with BigInt's getData).

Files swept this round:
- src/common/util.cpp - file_get_line static buffer (xrealloc -> manual
  grow with new char[]/memcpy/delete[]), 3 escape_* functions (xmalloc ->
  new char[]).
- src/bnetd/file_plain.cpp - esckey/escval allocations + paired delete[]
  cleanup (matches util.cpp's allocator change).
- src/common/packet.cpp - t_packet -> new/delete.
- src/common/peerchat.cpp - gs_peerchat_ctx -> new/delete.
- src/common/xstr.cpp - t_xstr + str grow buffer (new[]/memcpy/delete[]);
  removed common/xalloc.h include.
- src/common/conf.cpp - conf_set_str + conf_load_cmdline newkey ->
  new char[]/delete[] + std::string for cmdline working copies.
- src/bnetd/cmdline.cpp + src/d2cs/cmdline.cpp + src/d2dbs/cmdline.cpp -
  xfree of conf-managed tmp -> delete[] const_cast<char*>(tmp). Resolves
  the cross-TU coupling that previously deferred these sites.
- src/common/addr.cpp - t_addr/t_netaddr -> new/delete; xstrdup'd
  temp->str (const char*) -> new char[]; tstr/temp strtok working copies
  -> std::string with empty-check; addrlist_append tstr -> std::string.
- src/common/trans.cpp - t_trans entries -> new/delete; two xstrdup'd
  tmp buffers (exclude/include) -> shared std::string tmp_storage with
  empty-check pointer.
- src/common/xstring.cpp - hexstrdup, strtoargv (5 sites), arraytostr
  (4 sites), str_replace -> new char[]/delete[] with manual grow for
  xrealloc paths.
- src/common/hashtable.cpp - t_hashtable, t_entry, t_internentry,
  rows[] array - all new/delete (fully internal, no API surface change).

Skipped intentionally:
- src/common/bigint.cpp - 24 sites tied to BigInt's getData(int,...)
  whose ownership is held by handle_bnet.cpp and BnetSRP3. Needs a
  proper class refactor (move BigInt::segment to std::vector<bigint_base>
  and getData to return owned buffer via std::vector<unsigned char>).
- src/common/bnetsrp3.cpp - 7 sites pair with BigInt::getData's
  xmalloc'd return buffer; defer with bigint.cpp.

Validation:
- Legacy docker build: green (all 8 targets built).
- v3 docker build: green - 1225 assertions in 213 test cases pass;
  bniutils round-trip + 4 e2e smoke tests (bnftp/bnchat/bnstat/bnbot) OK.

Status:
- common/ effectively complete (only BigInt-coupled allocations remain).
- d2cs/d2dbs subtrees complete.
- bnetd/ still has ~50 files with ~700+ sites pending.
## Round 6 - BigInt + bnetsrp3 + account_wrap/handle_bnet (DONE)

- src/common/bigint.cpp: added static helper bigint_resize(uint32_t*, copy_count, new_count);
  6 ctor xmalloc -> new bigint_base[]{}, 13 xrealloc -> bigint_resize (DISCARD/PRESERVE),
  xfree(in)/xfree(segment) -> delete[], getData() returns new unsigned char[].
- Header note: bigint_base is a private typedef inside class BigInt; the file-scope
  helper uses std::uint32_t directly (same underlying type). Member storage is still
  raw owning pointer (bigint_base* segment) -- did NOT migrate to std::vector to keep
  bigint.h ABI unchanged; can be revisited later.
- src/common/bnetsrp3.cpp: username/password/userpass/raw_secret use new char[]/delete[].
- src/bnetd/account_wrap.cpp:166: account_get_rawattr_real result -> new char[]{}.
- src/bnetd/handle_bnet.cpp: 6 xfree call sites for account_salt/account_verifier/
  conn_client_proof/conn_server_proof converted to delete[] (with const_cast for
  const char* members).

Validation:
- Legacy docker build: green (all targets built, cached on rebuild).
- v3 docker build: green - 1225 assertions in 213 test cases; bnftp/bnchat/bnstat/bnbot
  e2e smokes OK.

## Round 7 - bnetd/connection.cpp (DONE)

- Added static helper conn_strdup(char const*) returning new char[] copy (paired delete[]).
- Bulk sweep converted all 47 xfree call sites:
  - xfree((void*)PTR); /* avoid warning */ -> delete[] const_cast<char*>(PTR);
  - xfree(c->protocol.w3.anongame) -> delete (single t_anongame struct)
  - xfree(qline) -> delete (single t_qline struct)
  - xfree(c->protocol.chat.ignore_list) -> delete[]
  - xfree((void*)connarray) -> delete[] (t_conn_entry array)
- Bulk sweep converted 25 allocators:
  - 18 xstrdup() -> conn_strdup()
  - xmalloc(sizeof(t_anongame)/t_qline) -> new T{}
  - 2 xrealloc on ignore_list -> inline grow/shrink with new[]/memcpy/delete[]
  - chatcharname = xmalloc(N) -> new char[N]
  - 2 proof = xmalloc(20) -> new char[20]{}
  - connarray = xmalloc(n*sizeof(t_conn_entry)) -> new t_conn_entry[n]{}

Validation:
- Legacy docker build: green (all targets built).
- v3 docker build: green - 1225 assertions in 213 test cases; e2e smokes OK.

## Round 8 - message.cpp + game.cpp + cross-TU game-result fixups (DONE)

- src/bnetd/message.cpp (75 sites):
  - 39 (char*)xmalloc(N) -> new char[N] (transient format buffers)
  - 4 xstrdup("") -> immediate-invoked lambda returning new char[1] with null term
  - 1 xrealloc growable buffer -> manual new[]/memcpy/delete[] grow
  - 4 growable arrays (packets/classes/dstflags/mclasses) - new T[]{} on first alloc;
    grow via IIFE: new T[n+1]{}; memcpy(p, field, n*sizeof(T)); delete[] field
  - 17 xfree calls -> delete[] (transient bufs), delete (single struct), delete[] arrays
- src/bnetd/game.cpp (64 sites):
  - Added static helper game_strdup() returning new char[] copy
  - new t_game{} ; xstrdup -> game_strdup (10 sites)
  - 6 growable arrays: connections/players/results/reported_results/report_heads/report_bodies
    using new T[n+1]{} + IIFE grow
  - new t_ladder_info[realcount]{} ; new char[...] for tempname/realname
  - 35 xfree -> delete[] for string buffers (with const_cast<char*>/<char**> as needed),
    delete[] for arrays, delete for single t_game
  - 1 plain free((void*)gametypes) (was odd legacy) -> delete[] const_cast<char*>(gametypes)
- Cross-TU coupling fixed:
  - src/bnetd/handle_bnet.cpp ~line 4730: t_game_result* results allocator switched to
    new t_game_result[N]{}; failure-path xfree -> delete[]
  - src/bnetd/handle_wol_gameres.cpp: t_wol_gameres_result struct + ->results array +
    failure cleanup all switched to new/delete[]

Validation:
- Legacy docker build: green (all targets built).
- v3 docker build: green - 1225 assertions in 213 test cases; e2e smokes OK.

## Round 9 - storage_file.cpp + ipban.cpp (DONE)

- src/bnetd/storage_file.cpp (56 sites):
  - Added static helper sf_strdup() returning new char[] copy
  - 9 xstrdup -> sf_strdup
  - new t_clan{} / new t_clanmember{} / new t_team{} (replacing xmalloc(sizeof(T)))
  - 8 (char*)xmalloc(N) -> new char[N] (pathname, tempname, clanfile, teamfile, etc.)
  - 30+ xfree(...) -> delete[] const_cast<char*>(...) for char* / delete for struct ptrs
  - file_free_info specifically: delete[] static_cast<char*>(const_cast<void*>(info))
    because t_storage_info is typedef'd as const void in storage.h
- src/bnetd/ipban.cpp (51 sites):
  - Added static helper ib_strdup() returning new char[] copy
  - 13 xstrdup -> ib_strdup (whole, cp, tipaddr, str, ipstr, e->info1, e->info2)
  - new t_ipban_entry{} (replacing xmalloc)
  - 35 xfree -> delete[] for char* working buffers, delete for struct pointers
  - e->info1 / e->info2 -> delete[] (they are char*)

Validation:
- Legacy docker build: green (all targets built).
- v3 docker build: green - 1225 assertions in 213 test cases; e2e smokes OK.

## Round 10 -- bnetd mid-tier sweep (handle_apireg + anongame_infos + icons)

- src/bnetd/handle_apireg.cpp (47 sites): added ar_strdup helper. xstrdup -> ar_strdup; (t_apiregmember*)xmalloc -> new t_apiregmember{}; xfree((void*)apiregmember->FIELD) -> delete[] const_cast<char*>(...); xfree(apiregmember) -> delete apiregmember; xfree(line) -> delete[] line.
- src/bnetd/anongame_infos.cpp (40 sites): added ai_strdup helper. Struct allocs new t_anongame_infos_DESC{} / new t_anongame_infos_data_lang{} / new t_anongame_infos_data{} / new t_anongame_infos{}. Arrays: new char*[count]{} for anongame_infos_URL and descs. (char*)xmalloc(N) -> new char[N] for ladr_data, tmpdata, (*dest). Struct ptrs: delete X (no const_cast). char** contents: delete[] X[i] (no const_cast).
- src/bnetd/icons.cpp (34 sites): added ic_strdup helper. xstrdup -> ic_strdup; (t_iconset_info*)xmalloc -> new t_iconset_info{}; same for t_icon_info / t_icon_var_info. char* member free: delete[] field; const char* stats: delete[] const_cast<char*>(stats); struct ptr free: delete option / delete iconset / delete icon_var.
- Gotcha: bulk regex made anongame_infos_URL (char**) and descs (char**) get delete[] const_cast<char*>(...) -- fixed to plain delete[] (and contents char* -> plain delete[]).
- Validation: legacy docker build OK; v3 docker build OK; 1225 assertions / 213 test cases; e2e smokes (bnftp, bnchat, bnstat, bnbot) all OK.

## Round 11 -- bnetd sweep (sql_dbcreator + channel + tournament)

- src/bnetd/sql_dbcreator.cpp (34 sites): added sd_strdup. structs t_column/t_sqlcommand/t_table/t_db_layout -> new T{}; xstrdup -> sd_strdup; (char*)xmalloc -> new char[N]; xfree members (char*) -> delete[]; struct ptrs -> delete.
- src/bnetd/channel.cpp (32 sites): added ch_strdup. t_channel/t_channelmember -> new T{}; xstrdup -> ch_strdup; const-char* members (name/shortname/country/realmname) -> delete[] const_cast<char*>(...); char* members (logname/gameExtension) -> delete[]; member/channel/curr/temp struct ptrs -> delete; newname/channelname char* -> delete[]; nested-paren xmalloc for logname patched manually.
- src/bnetd/tournament.cpp (28 sites): added tn_strdup. t_tournament_user/t_tournament_info/std::tm -> new T{}; (char*)xmalloc -> new char[N]; xfree on all char* fields and locals -> delete[]; struct ptrs -> delete.
- Validation: legacy docker build OK; v3 docker build OK; 1225 assertions / 213 test cases; e2e smokes (bnftp, bnchat, bnstat, bnbot) all OK.


## Round 12 -- Big sweep: remaining 20 small files (xalloc -> modern C++)

Final wave of bnetd xalloc elimination. All remaining .cpp files with
xmalloc/xfree/xstrdup/xrealloc/xcalloc usage have been converted.

Converted in this round (18 files; friends.cpp & game.cpp had only
comments referring to historical xalloc -- skipped):

- account.cpp        -- ac_strdup helper. t_account -> new T{}. account->name char* -> delete[]. account -> delete.
- command.cpp        -- cmd_strdup helper. str.p/data.p char* fields -> delete[]. (char*)text -> const_cast<char*>.
- command_groups.cpp -- cg_strdup helper. t_command_groups -> new T{}. entry->command -> delete[]. entry -> delete.
- handle_bot.cpp     -- handlebot strdup helper. testpass char* -> delete[].
- handle_telnet.cpp  -- handletelnet strdup helper. testpass char* -> delete[].
- handle_d2cs.cpp    -- (char*)xmalloc(nested-paren) -> new char[...]. xfree temp -> delete[].
- handle_irc_common.cpp -- irc_common_strdup helper. line char* + bnet_command char* -> delete[].
- handle_wol.cpp     -- wol_strdup helper. pass char* -> delete[].
- handle_wserv.cpp   -- filestring (char const*) -> delete[] const_cast<char*>.
- helpfile.cpp       -- (char*)xmalloc(length+1) -> new char[length+1]. xrealloc -> IIFE memcpy. buffer -> delete[].
- i18n.cpp           -- i18n_strdup helper for languages.push_back lang_name.
- ladder.cpp         -- t_xpcalc_entry/t_xplevel_entry arrays -> new T[N]{}. xrealloc -> IIFE memcpy + delete[].
- ladder_calc.cpp    -- unsigned int arrays (rating/sorted) -> new unsigned int[count]{}. xfree -> delete[].
- luainterface.cpp   -- lua_strdup helper. t_game -> new T{}.
- support.cpp        -- (char*)xmalloc(nested-paren) -> new char[...]. namebuff -> delete[].
- team.cpp           -- t_team -> new T{}. team -> delete.
- timer.cpp          -- t_timer -> new T{}. timer -> delete.
- userlog.cpp        -- ul_strdup helper. temp -> delete[]. lines[] entries paired.

Skipped (only comments, no code change needed):
- friends.cpp        -- line 193 comment only.
- game.cpp           -- lines 1676, 1682 comments only.

Validation:
- Legacy docker build: PASSED (bnetd_legacy / bnetd / d2cs / d2dbs all built).
- v3 docker build:     PASSED (1225 assertions / 213 test cases + 4 e2e smokes: bnftp / bnchat / bnstat / bnbot).

Round 12 status: COMPLETE. bnetd xalloc elimination 100% done across
all .cpp files in src/bnetd.

## Round 13 -- Mop-up: handle_d2cs.cpp residual + attr.h header

Started refactoring-plan-legacy-common phase. Pre-flight audit shows
src/common/, src/d2cs/, src/d2dbs/, src/client/, src/tools/ already have
zero xalloc usage. Only two residuals remained anywhere in src/:

- src/bnetd/handle_d2cs.cpp -- multi-line nested-paren (char*)xmalloc(...)
  at line 296 (missed by single-line regex). Manual edit to new char[...].
- src/bnetd/attr.h -- inline header functions attr_create / attr_destroy /
  attr_set_val. Added attr_strdup helper, t_attr -> new T{}, char const*
  key/val -> delete[] const_cast<char*>, attr -> delete. Replaced
  #include "common/xalloc.h" with #include <cstring>.

Validation:
- Legacy docker build: PASSED (bnetd_legacy / bnetd / d2cs / d2dbs).
- v3 docker build:     PASSED (1225 assertions / 213 test cases + 4 e2e).

bnetd + all peer subsystems: 100% xalloc-free at source level.
Remaining xalloc references in tree are: legacy_compat.hpp shim,
unit tests using the shim, and doc/comment text only.

Next refactoring-plan-legacy-common steps to consider:
- Step 1: create src/v3/infra/compat/
- Step 2: create src/v3/infra/crypto/ (legacy_crypto already exists)
- Step 3: migrate protocol definitions
- Step 5: migrate type utilities to core/
- Step 7: eliminate t_list / t_hashtable in legacy consumers