# Bug Hunt — BNFTP + SID_GETFILETIME / file serving

Subsystem: Battle.net File Transfer (BNFTP) and BNet-protocol file-time / file-info serving.

ORIGINAL: `/home/cnupt/work/pvpgn-server`
CURRENT v3: `/home/cnupt/work/pvpgn`

Key reference files
- Original BNFTP serve: `src/bnetd/file.cpp`, dispatch `src/bnetd/handle_file.cpp`, init `src/bnetd/handle_init.cpp`
- Original structs: `src/common/file_protocol.h`, init class byte `src/common/init_protocol.h`
- Original SID_GETFILETIME (FILEINFOREQ): `src/bnetd/handle_bnet.cpp:1429` (`_client_fileinforeq`)
- Original timestamp: `src/common/bnettime.cpp`, `src/common/bn_type.cpp` (`bn_long_set_a_b`)
- v3 BNFTP FSM: `src/protocol/file/src/bnftp_fsm.cpp`, header `.../include/protocol/file/bnftp_fsm.hpp`
- v3 codec/wire: `src/protocol/file/src/codec.cpp`, `.../wire_types.hpp`
- v3 shared-port dispatch: `src/app/bnetd/src/main/bnet_bnftp_dispatch.cpp`
- v3 SID_GETFILETIME handler: `src/protocol/bnet/src/fsm/fsm_auth.cpp:182` (`on(FileInfoRequest)`)

---

## FINDING 1 — BNFTP init class byte (0x02) is NOT consumed before the FSM parses the file request  [CRIT / BUG]

**Severity:** Critical (BNFTP server completely non-functional on the real wire).
**Classification:** BUG.

**Protocol fact.** A BNFTP connection begins with a single init-class octet `0x02`
(`CLIENT_INITCONN_CLASS_FILE`) and only *then* the `CLIENT_FILE_REQ` packet.
This is documented in the v3 mock and the v3 client:
- `tests/e2e/fake_bnftp_server.py:48-52` — `recv_exact(conn, 1)` expects `b"\x02"` first, then the 4-byte header.
- `src/tools/client/bnftp_v3.cpp:362` — client sends `send_init_classbyte(... init_class::File)` (i.e. `0x02`) before the request.
- Original confirms the byte is consumed by the init handler, which sets the connection class and does NOT pass it on:
  `src/bnetd/handle_init.cpp:77-80`
  ```cpp
  case CLIENT_INITCONN_CLASS_FILE:
      ...
      conn_set_class(c, conn_class_file);
  ```
  After that, `handle_file_packet` (`src/bnetd/handle_file.cpp:63`) parses the *next* packet, which starts cleanly with `{size,type}`.

**v3 ref.** `src/app/bnetd/src/main/bnet_bnftp_dispatch.cpp:142-158`
```cpp
} else {
    // BNFTP protocol (or unknown — let BnftpFsm reject it)
    ...
    auto fsm = std::make_shared<protocol::file::BnftpFsm>(ctx, cfg_.data_dir.string());
    // Replay buffered bytes
    auto sp = std::span<const std::byte>(pbuf->data(), pbuf->size());  // includes the 0x02 byte
    (void)fsm->on_bytes(sp);
```
The dispatch peeks only `first` (`(*pbuf)[0]`) to choose BNFTP vs BNet (`first != 0xFF`), but then replays the **entire** buffer — including the leading `0x02` — into `BnftpFsm::on_bytes`. The FSM never skips it:
`src/protocol/file/src/bnftp_fsm.cpp:138-147` treats `buf_[0..1]` as the LE `size` and `buf_[2..3]` as `type`:
```cpp
std::uint16_t pkt_size = p[0] | (p[1] << 8);
std::uint16_t pkt_type = p[2] | (p[3] << 8);
```

**Divergence / impact.** With the `0x02` prefix present, the parsed header is shifted by one byte:
the real packet `02 | [sizeLo sizeHi] [typeLo typeHi] ...` is misread (`pkt_size`'s low byte = `0x02`, type bytes wrong), so `pkt_type != kClientFileReq (0x0100)` and the FSM hits the "unknown request type → close" path (`bnftp_fsm.cpp:160-165`) and silently drops the connection. **Every real BNFTP download fails.** (In the rare case the shifted bytes happen to look like type 0x0100, the filename/offset would be parsed from the wrong offsets — corrupt serve.)

**Why tests don't catch it.**
- Unit tests (`tests/unit/protocol/file/bnftp_fsm_test.cpp`) feed packets built by `make_file_req()` that begin directly with `{size,type}` — **no `0x02` prefix** — so the FSM-in-isolation passes.
- The e2e smoke (`scripts/v3-e2e-bnftp-smoke.sh`) drives the v3 **client** against the **Python mock server**; the real v3 server path (dispatch → BnftpFsm) is never exercised end-to-end.

**Proposed fix.** In the BNFTP branch of the dispatch, drop the leading init-class byte before replaying, e.g. feed `pbuf->data()+1, pbuf->size()-1` to `fsm->on_bytes` (the rewired `on_bytes` for subsequent reads is already correct, since the init byte only appears once at the very start). Alternatively, have `BnftpFsm` consume/expect a leading `0x02` in its `AwaitingRequest` state. Add an e2e test that drives the real dispatch+FSM (not the mock) including the init byte.

---

## FINDING 2 — SID_GETFILETIME (SID 0x33 / FILEINFOREQ) handler is a no-op; no reply is sent  [HIGH / NOT-IMPLEMENTED]

**Severity:** High (TOS/newaccount/icon flows that gate on the file-time reply may hang or mis-render).
**Classification:** NOT-IMPLEMENTED.

**Original ref.** `src/bnetd/handle_bnet.cpp:1429-1467` (`_client_fileinforeq`):
builds `SERVER_FILEINFOREPLY`, echoes `type` and `unknown2` from the request, fills the file mod-time via `file_to_mod_time(c, filename, &...timestamp)`, appends the filename string, and pushes the reply.
```cpp
bn_int_set(&rpacket->u.server_fileinforeply.type, bn_int_get(packet->u.client_fileinforeq.type));
bn_int_set(&rpacket->u.server_fileinforeply.unknown2, bn_int_get(packet->u.client_fileinforeq.unknown2));
file_to_mod_time(c, filename, &rpacket->u.server_fileinforeply.timestamp);
packet_append_string(rpacket, filename);
conn_push_outqueue(c, rpacket);
```

**v3 ref.** `src/protocol/bnet/src/fsm/fsm_auth.cpp:182-186`:
```cpp
core::Status<> BnetFsm::on(const FileInfoRequest&) {
    // GETFILETIME may be sent ... accept in any non-Closing state.
    return core::ok();
}
```
No `FileInfoReply` is ever constructed or sent in the FSM/application layer (the `encode(FileInfoReply)` exists at `src/protocol/bnet/src/codec/codec_auth.cpp:341` but is unused server-side; only client-decode test paths use it).

**Divergence.** Original always answers GETFILETIME with the echoed request id (`type`) + FILETIME + filename; v3 answers with nothing. Clients that issue SID_GETFILETIME and wait for the reply (e.g. for TOS / icon timestamps) get no response.

**Proposed fix.** Wire `on(FileInfoRequest)` to resolve the requested filename's mod-time (using the v3 files dir) and emit a `FileInfoReply` echoing `type`/`unknown2` and the filename. If the file is missing, original still replies (timestamp from `file_to_mod_time`, which returns -1 / leaves timestamp untouched but still sends) — replicate "always reply".

---

## FINDING 3 — BNFTP v2 handshake (CLIENT_FILE_REQ2 / 0xdeadbeef / CLIENT_FILE_REQ3) not implemented  [MEDIUM / NOT-IMPLEMENTED]

**Severity:** Medium (War3 BNFTP two-step downloads unsupported).
**Classification:** NOT-IMPLEMENTED.

**Original ref.** `src/bnetd/handle_file.cpp:82-118`:
- `CLIENT_FILE_REQ2` (0x0200): server replies with a raw `SERVER_FILE_UNKNOWN1` = `0xdeadbeef`, sets `conn_state_pending_raw`.
- `CLIENT_FILE_REQ3` (0x0000): raw record (no header) followed by filename read via `psock_recv`, then `file_send(...)`.
Structs: `src/common/file_protocol.h:92-112`.

**v3 ref.** `src/protocol/file/src/bnftp_fsm.cpp:160-165` — any `pkt_type != kClientFileReq (0x0100)` closes the connection. The codec (`codec.cpp:36-61`) only decodes `kClientFileReq`/`kServerFileReply`; it returns `Unimplemented` for `0x0200`. The wire constants exist (`wire_types.hpp:30-31`, `ClientFileReq2`, `ClientFileReq3`) but no FSM path handles them. The header comment (`bnftp_fsm.hpp:6-7`) explicitly says "Implements the BNFTP v1 request/response protocol" only.

**Divergence.** W3 clients using the REQ2→0xdeadbeef→REQ3 sequence cannot download via v3.

**Proposed fix.** Either implement the REQ2/REQ3 two-step in `BnftpFsm` (send `0xdeadbeef`, switch to a raw-filename state, then serve), or document the intentional drop if W3 BNFTP is out of scope. Currently presents as silent connection close.

---

## FINDING 4 — Localized / aliased file resolution dropped (termsofservice, tos_, newaccount, etc.)  [MEDIUM / NOT-IMPLEMENTED]

**Severity:** Medium (per-language TOS / chathelp / matchmaking files no longer served; clients get size-0).
**Classification:** NOT-IMPLEMENTED (behavioral divergence).

**Original ref.** `src/bnetd/file.cpp:57-112` (`file_find_localized` + `requestfiles[]` alias table) and `file_get_info:137-149`: it first resolves localized aliases (e.g. `termsofservice-ruRU.txt`, `tos_USA.txt` → i18n file by gamelang/country), and only falls back to `prefs_get_filedir()/rawname` if no localized match.

**v3 ref.** `src/protocol/file/src/bnftp_fsm.cpp:214-224`: builds path strictly as `files_dir_ / filename` and stats it; no alias/i18n resolution. Missing → size-0 reply.

**Divergence.** Requests for the alias-prefixed localized names (which real clients send) won't resolve unless a file literally named e.g. `termsofservice-ruRU.txt` exists in `data_dir`. Original mapped these to localized i18n files. Same gap affects the BNet SID_GETFILETIME path if/when it is implemented (Finding 2), since original `file_to_mod_time` routes through the same `file_get_info` localization.

**Proposed fix.** Port the `requestfiles[]` alias + i18n lookup, or document the intentional simplification. Without it, localized TOS serving regresses.

---

## FINDING 5 — Filename length cap reduced 2048 → 128  [LOW / UNSURE]

**Severity:** Low.
**Classification:** UNSURE (likely intentional hardening; minor compat risk).

**Original ref.** `src/common/field_sizes.h:55` `MAX_FILENAME_STR = 2048`; used both in BNFTP (`handle_file.cpp:67`) and FILEINFOREQ (`handle_bnet.cpp:1441`).
**v3 ref.** `src/protocol/file/src/bnftp_fsm.cpp:35` `kMaxFilenameLen = 128`; `is_safe_filename` rejects names > 128 (`bnftp_fsm.cpp:55-60`). The code comment claims it "matches legacy MAX_FILENAME_STR" — that is inaccurate (legacy is 2048).

**Divergence.** Names between 129 and 2047 chars that the original would serve now get a size-0 reply. Unlikely in practice but the comment is misleading.

**Proposed fix.** Either raise to 2048 to match, or keep 128 and correct the comment. Low priority.

---

## THINGS THAT MATCH (verified, no bug)

### M1 — FILETIME (u64) computation and byte order  ✔
- Original (`bnettime.cpp` + `bn_long_set_a_b`): builds the 64-bit value `ticks = (unix_secs + 11644473600) * 1e7`, split into `u = ticks/2^32` (high) and `l = ticks` (low 32), then `bn_long_set_a_b(out, u, l)` writes **low 32 bits first** (bytes 0-3 = `l`), **high 32 bits next** (bytes 4-7 = `u`) — i.e. 64-bit little-endian FILETIME. (`bn_type.cpp:495-502`, epoch/scale constants `bnettime.cpp:42,86-87`.)
- v3 (`bnftp_fsm.cpp:46-51` `mtime_to_filetime` + `write_u64le` `:93-96`): `(t + 11644473600) * 10000000`, written low-32 then high-32. **Identical semantics**, and v3 is integer-exact whereas the original goes through `double` (minor precision drift only in the original).

### M2 — SERVER_FILE_REPLY wire layout  ✔
Original `t_server_file_reply` (`file_protocol.h:131-139`): `{u16 size, u16 type=0x0000}`, `u32 filelen`, `u32 adid`, `u32 extensiontag`, `u64 timestamp`, cstring filename.
v3 `send_reply_header` (`bnftp_fsm.cpp:255-291`) and `encode(ServerFileReply)` (`codec.cpp:95-103`) emit exactly that order, all LE, NUL-terminated filename, `type=0x0000`. Fixed prefix 24 bytes both sides. **Matches.**

### M3 — CLIENT_FILE_REQ parse layout  ✔
Original `t_client_file_req` (`file_protocol.h:69-79`): header + `archtag, clienttag, adid, extensiontag, startoffset` (5×u32) + `timestamp` (u64) + filename. v3 parses the same field order/offsets (`bnftp_fsm.cpp:177-188`, `codec.cpp:37-46`). The codec/FSM read `ad_id`/`extension_tag`/`start_offset` and echo `ad_id`+`extension_tag` into the reply, matching original `file_send(... adid, etag, startoffset ...)`. **Matches.**

### M4 — Header field order {size, type} (opposite of BNet)  ✔
Both treat the FILE-class header as `{u16 size, u16 type}` LE (original `t_file_header` `file_protocol.h:33-37`; v3 `parse_header` `codec.cpp:11-23` and `FileHeader` `wire_types.hpp:18-24`). This is correctly the **opposite** of the BNet `{type,size}` order. **Matches.**

### M5 — File-not-found → size-0 reply (client must not hang)  ✔
Original `file_send` (`file.cpp:188-235`): on stat/open failure sets `filelen = 0` and still sends the reply header. v3 `handle_file_request` (`bnftp_fsm.cpp:220-224`) sends a zero-length reply header on `file_size` error. **Matches** (semantics preserved).

### M6 — start_offset clamping  ✔
Original: if `startoffset >= filelen`, keeps real filesize and sends no data (`file.cpp:239-247`). v3: clamps `start_offset` to `file_size`, computes `send_len = file_size - start_offset` (so 0 past EOF) and sends no data when 0 (`bnftp_fsm.cpp:237-245`). Slightly different reply `filelen` semantics: **v3 reply `filelen` = bytes being sent (size - offset)** whereas original reply `filelen` = full file size; but in both, past-EOF sends no data. Note: this is also a subtle wire difference — see note below.

### M7 — Path-traversal protection (`/`, `\`)  ✔ (NO security bug)
- Original `file_get_info` (`file.cpp:131-134`) **throws** if rawname contains `/` or `\`.
- v3 `is_safe_filename` (`bnftp_fsm.cpp:55-60`) rejects `/`, `\`, `\0`, empty, and >128. An absolute path or `../x` contains `/` (or `\`) and is rejected → size-0 reply. `std::filesystem::path(dir) / "/etc/passwd"` cannot be reached because the `/` is rejected first. **Traversal is blocked in v3.** (Covered by tests `bnftp_fsm_test.cpp:259-294`.)
- Minor note: original rejects via exception (logs + returns error, no reply); v3 sends a size-0 reply for the bad name. Both are safe; v3 is arguably friendlier. NOT a bug.

---

## SECONDARY NOTE (low) — reply `filelen` meaning under start_offset

Original `SERVER_FILE_REPLY.filelen` carries the **full** file size (`file.cpp:218-253`: `filelen` from stat is what gets `bn_int_set` into the reply, independent of `startoffset`), then it `fseek`s and streams `filelen - startoffset` bytes. v3 puts **`file_size - start_offset`** into the reply's `filelen` (`bnftp_fsm.cpp:239-242`). For the common case `start_offset == 0` these are equal; for resume (`start_offset > 0`) the reply field differs from the original. Classification: UNSURE/possible-minor-BUG — worth confirming against a real client's resume expectation. Listed separately from M6 because it is a genuine on-wire value divergence, not just internal bookkeeping.

---

## Summary of severities
- CRIT/BUG: Finding 1 (init `0x02` byte fed into FSM → all real BNFTP downloads fail; untested by both unit and e2e because tests bypass the dispatch/init-byte).
- HIGH/NOT-IMPL: Finding 2 (SID_GETFILETIME no-op, no reply).
- MEDIUM/NOT-IMPL: Finding 3 (REQ2/REQ3 W3 two-step), Finding 4 (localized/alias files).
- LOW: Finding 5 (128 vs 2048 cap + misleading comment), Secondary note (resume `filelen` value).
- MATCHES: header order, REQ/REPLY layouts, FILETIME math+byte order, file-not-found size-0, offset past-EOF, **path traversal protection (no security hole)**.

## Wave 67: BNFTP body-drop fixed (graceful close)
A BNFTP mock client (tests/diff/bnftp_client.py, diff_bnftp.py) found v3 served the
reply header but ZERO body bytes (downloads broken). Root cause: BnftpFsm::
try_dispatch closed the connection synchronously after queueing header+body writes;
TcpSession close()/send() post onto the strand FIFO so close ran before the body
write. Fix: TcpSession graceful close — close() defers (close_after_flush_) when
writes are pending; the write-completion handler runs deliver_close once the queue
drains (idle timer = stall backstop). v3 now delivers full bodies 1..65536 bytes,
matching the oracle. Minor accepted diff: v3 closes after serving one file; the
oracle lingers (multi-file-per-connection). Wire format documented in diff_bnftp.py.
