# Malformed / Hostile Input Safety Audit — v3 PvPGN rewrite

Scope: parsing of attacker-controlled bytes. Connection FSM hand-rolled helpers,
bnet codec `Reader`, and per-protocol fsm/codec parsers (d2cs, d2dbs, d2gs,
d2save, telnet, irc, wol, wolgameres, file, udp).

Read-only audit. No source edited, no build run.

---

## Summary

- HIGH/CRIT issues found: **2** (both unbounded line-buffer accumulation → remote OOM/DoS from an unauthenticated client).
- The core bounded `Reader` and the entire **bnet codec** layer are well hardened
  (every count/length field is capped before `resize()/reserve()`; every read is
  bounds-checked). This is the strongest part of the rewrite.
- The connection-FSM hand-rolled helpers (`read_le32`, `read_cstring`, offset
  arithmetic for STARTADVEX / GETADVLISTEX) are all correctly bounded — verified
  safe.
- The two findings are NOT in the codecs themselves but in the **line-based
  session drivers** (IRC and telnet), where a growable `std::string` accumulates
  socket bytes with no cap until a newline arrives. WOL got this right; IRC and
  telnet did not.

---

## FINDINGS

### [HIGH] IRC TCP session: unbounded `rx_buf_` growth (remote OOM DoS)

- Classification: **BUG**
- v3 ref: `src/app/bnetd/include/app/bnetd/irc_tcp_session.hpp:128-148`

```cpp
tcp_->set_on_bytes([self](core::ByteView bv) {
    // Accumulate bytes into the line buffer
    self->rx_buf_.append(
        reinterpret_cast<const char*>(bv.data()), bv.size());   // <-- no cap

    while (true) {
        auto frame_result = protocol::irc::try_parse_line(self->rx_buf_);
        if (!frame_result) break;  // NeedMore — returns when no '\n' present
        ...
    }
});
```

`protocol::irc::try_parse_line` (`src/protocol/irc/src/codec.cpp:27-38`) returns
`OutOfRange "irc: incomplete line"` whenever the buffer contains no `'\n'`. Until
a newline is seen, nothing is ever erased from `rx_buf_`, and every inbound chunk
is appended.

- Trigger (malformed input): an **unauthenticated** client opens the IRC/WOL TCP
  port and sends a continuous stream of bytes containing no `'\n'` (e.g. 1 GiB of
  `'A'`). `rx_buf_` grows to match, exhausting server memory. No login required —
  `set_on_bytes` is wired at `start()`, before any registration.
- Original protection: legacy `irc.cpp` parses lines into fixed `char
  data[MAX_IRC_MESSAGE_LEN]` buffers; the line length is inherently bounded by a
  fixed-size buffer, so an over-long line cannot grow allocation without bound.
- Proposed fix: cap `rx_buf_` exactly as WOL does. After the `append`, if
  `rx_buf_.size()` exceeds a sane maximum (e.g. IRC line limit 512 × a small
  factor, matching `wol_fsm.cpp`'s `kMaxLineLen * 4`), close the session and stop:

```cpp
self->rx_buf_.append(...);
if (self->rx_buf_.size() > kMaxIrcLine * 4) {  // e.g. 512*4
    self->close();
    return;
}
```

  (See `src/protocol/wol/src/wol_fsm.cpp:112-117` for the existing model.)

---

### [HIGH] Telnet session: unbounded `line_buffer_` growth (remote OOM DoS)

- Classification: **BUG**
- v3 ref: `src/integration/telnet/src/telnet_session.cpp:21-47`

```cpp
core::Result<void, core::Error> TelnetSession::feed(core::ByteView data) {
    for (auto b : data) {
        const uint8_t byte = static_cast<uint8_t>(b);
        if (byte == IAC) continue;
        if (byte == '\r' || byte == '\n') {
            if (!line_buffer_.empty()) { handle_line(line_buffer_); line_buffer_.clear(); }
            continue;
        }
        if (byte >= 32 && byte < 127) {
            line_buffer_ += static_cast<char>(byte);   // <-- no cap
        }
    }
    return core::Result<void, core::Error>{};
}
```

- Trigger (malformed input): an unauthenticated telnet client sends a stream of
  printable ASCII (0x20–0x7E) with no `\r`/`\n`. `line_buffer_` grows without
  bound → OOM. Same class of bug as the IRC session.
- Original protection: legacy `handle_telnet.cpp` reads into fixed-size buffers.
- Proposed fix: add a max-line guard in the accumulation branch, mirroring WOL:

```cpp
if (byte >= 32 && byte < 127) {
    if (line_buffer_.size() >= kMaxTelnetLine) {   // e.g. 1024
        // drop / disconnect rather than grow unbounded
        line_buffer_.clear();   // or close the session
        continue;
    }
    line_buffer_ += static_cast<char>(byte);
}
```

---

## SAFE — VERIFIED (bounded parsers)

### Connection FSM helpers — `connection_fsm_internal.hpp`
- `read_le32(s, off)` (line 39): guards `offset + 4 > s.size()`, returns 0 on
  OOB. All call-site offsets are small literals (0,1,4,8) or derived from
  `payload`-bounded string sizes, so `offset + 4` cannot integer-overflow in
  practice. **SAFE.**
- `read_cstring(s, off)` (line 50): guards `offset >= s.size()`, scans with
  `memchr` only within `[off, size)`, returns `{begin,end}` if no NUL. No
  over-read. **SAFE.**

### Connection FSM handlers
- `on_start_game` / `on_join_game` (`connection_fsm_inchannel.cpp:233-237`,
  `281-285`): the `pw_offset = 16 + game_name.size() + 1` and
  `stats_offset = pw_offset + password.size() + 1` arithmetic is bounded —
  `game_name.size() <= payload.size()-16`, so each derived offset stays within
  `payload.size()+1`, and `read_cstring` rejects `offset >= size`. No over-read,
  no unbounded loop, no allocation driven by a raw length field. **SAFE.**
- `on_logon_request` (`connection_fsm_connecting.cpp:144`): `memcpy(hash, payload+8,
  20)` is guarded by `payload.size() >= 28`. **SAFE.**
- `on_auth_accountlogon` / `on_auth_accountlogonproof`
  (`connection_fsm_authenticating.cpp:54-55, 132-133`): both memcpys use
  `std::min(payload.size(), N)` into a fixed `std::array<…,N>`. Safe even on an
  empty payload. **SAFE.**
- `on_d2_char_select` (`connection_fsm_ingame.cpp:63`) checks `payload.size() < 3`;
  `on_warcraft_general` (`:90`) checks `< 5`. **SAFE.**
- `dispatch` ping echo (`connection_fsm.cpp:51`): `read_le32(payload,0)` returns 0
  on short payload — no crash. **SAFE.**

### bnet codec `Reader` — `src/protocol/common/include/protocol/common/reader.hpp`
- `read_bytes(n)` (line 73): `if (remaining() < n) return short_()`. **bounded.**
- `read_le/read_be<T>` (lines 52/63): `if (remaining() < sizeof(T))`. **bounded.**
- `read_cstring()` (line 82): scans only `pos_..buf_.size()`, returns
  `OutOfRange "unterminated string"` if no NUL — never over-reads. **bounded.**
- `skip(n)` (line 41): `remaining() < n` checked. **SAFE-VERIFIED.**

### bnet codec count/length caps (all capped before resize/reserve)
- `codec_auth.cpp:98` cdkey_count > 8 → reject.
- `codec_game.cpp:36` game_count > 1024 → reject; `:218` GAMEREPORT
  `count > remaining()/4` → reject (bounded to payload).
- `codec_ladder.cpp:66` count > `kLadderListLimit` → reject.
- `codec_realm.cpp:64,201` count > `kRealmListLimit`; `:173` EXTRAWORK len >
  `kExtraWorkMaxLen (32768)` → reject before `read_bytes`.
- `codec_friends.cpp:21` count > 200; `:127,150` arranged-team friend/invite
  counts capped.
- `codec_chat.cpp:75,149` CHANNELLIST count capped.
- `codec_clan.cpp:60,166,273` friend_count / member_count capped before reserve.
  **All SAFE-VERIFIED.**

### Per-protocol parsers
- **d2dbs** (`codec.cpp:56-68, 118-124`): `datalen` is `uint16` (max 65535);
  `read_bytes(datalen)` is bounds-checked and runs **before** `m.data.resize(datalen)`,
  so an oversized length fails at the read, not the allocation. **SAFE.**
- **d2cs** (`codec.cpp:202-212, 222-239`): `currchar` capped (>16, >64) before
  `reserve`; per-name `read_cstring()` is the bounded `Reader` variant. **SAFE.**
- **d2gs** (`codec.cpp:88-89, 140`): uses bounded `Reader`; `read_bytes(128)` and
  `read_bytes(r.remaining())` are checked. **SAFE.**
- **wolgameres** (`codec.cpp:64-76`): `while(!r.empty())` loop; each iteration
  reads a `uint16` len then `read_bytes(len)` (bounds-checked) — loop makes
  forward progress and terminates. **SAFE.**
- **d2save** (`codec.cpp:47-162`): single up-front `data.size() < MIN_FILE_SIZE`
  guard where `MIN_FILE_SIZE = 335`; total fixed-header bytes read = 191 < 335, so
  every `memcpy(data.data()+offset, …)` and `data[offset++]` stays in bounds. Not
  network-facing from unauth clients, but verified bounded regardless. **SAFE.**
- **file/bnftp** (`codec.cpp`): uses bounded `Reader` (`read_cstring`, subspan).
  **SAFE.**

### Line tokenisers (text protocols)
- **irc** (`codec.cpp:40-92`), **telnet** (`codec.cpp:18-40`), **wol**
  (`codec.cpp:45-58, 240-246, 345-364`): all token loops are `while (i <
  line.size() …)` / consume-and-shrink (`next_token` always removes ≥1 char or
  returns empty on an empty view); no over-read, no infinite loop. The
  `decode()`-level parsing of an already-framed line is **SAFE**. (The DoS is in
  the *framing/accumulation* drivers above, not in these tokenisers.)
- **wol fsm** (`wol_fsm.cpp:108-118`): correctly caps `line_buf_` at
  `kMaxLineLen * 4` and disconnects — this is the model the IRC/telnet sessions
  should follow.

---

## Notes / non-issues considered

- `read_le32`'s `offset + 4` could in theory wrap for a `size_t` offset near
  `SIZE_MAX`, but no call site passes an attacker-controlled raw offset; all
  offsets are small literals or `payload`-bounded string-length sums. Not
  exploitable. Listed for completeness.
- Telnet `try_parse_line` (`protocol/telnet/src/codec.cpp`) exists but the live
  telnet accumulation path used by `TelnetSession::feed` is the hand-rolled loop
  audited above; both share the same missing-cap class.
