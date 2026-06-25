# Bug-hunt: connection lifecycle / timers (idle, nullmsg, latency, flood)

Subsystem: idle/read timeout, NULL keepalive, null-packet timer, flood/packet-rate
limit, connection-class transitions, ping/latency measurement.

ORIGINAL: `/home/cnupt/work/pvpgn-server`  (`src/bnetd/connection.cpp`, `server.cpp`, `handle_bnet.cpp`)
CURRENT v3: `/home/cnupt/work/pvpgn`

Method: read-only comparison. No edits, no build.

---

## Summary of verdicts

| # | Topic | Severity | Class |
|---|-------|----------|-------|
| 1 | Idle deadline IS reset on inbound bytes (the key check) | n/a | MATCHES (good) |
| 2 | No server-initiated keepalive (ECHOREQ / nullmsg) → 300s idle drop of alive-but-quiet clients | MEDIUM | BUG / NOT-IMPLEMENTED |
| 3 | Latency/ping measurement (SERVER_ECHOREQ round-trip) not implemented; `latency` never populated | LOW-MED | NOT-IMPLEMENTED |
| 4 | Connection-level packet flood limit (`packet_limit`, drop on outqueue overflow) has no v3 analogue | LOW-MED | NOT-IMPLEMENTED |
| 5 | SID_PING (0x25) echo IS implemented and matches client-ping semantics | n/a | MATCHES |
| 6 | `initkill_timer` (stale-init reaper) absent, but subsumed by the 300s idle deadline | LOW | INTENTIONAL |
| 7 | Chat message quota (`Quota`) exists but is NOT wired into the live chat path | LOW | UNSURE / NOT-WIRED |

---

## Finding 1 — Idle-read deadline IS reset on every inbound packet  (MATCHES — the key check passes)

The config-defaults hunt flagged a concern: v3 introduces a per-protocol
`[net.timeouts]` 300s idle-read deadline with no original analogue, and asked
whether the deadline is **absolute** (would wrongly drop idle-but-alive BNCS
clients that only send periodic NULLs) or **reset on activity**.

VERDICT: the deadline is correctly reset on **any** inbound bytes, including a
SID_NULL keepalive.

v3 ref — `src/infra/net/src/tcp_session.cpp:61-77`:
```cpp
void TcpSession::do_read() {
    ...
    socket_.async_read_some(..., [self](const error_code& ec, std::size_t n) {
        ...
        if (n > 0 && self->on_bytes_) self->on_bytes_(...);
        if (self->closed_) return;
        self->arm_idle_timer();   // bytes seen → reset the idle deadline
        self->do_read();
    }));
}
```
`arm_idle_timer()` (`tcp_session.cpp:29`) calls `expires_after(idle_timeout_)`,
which cancels the prior `async_wait` (old handler sees `operation_aborted` and
bails). So a SID_NULL — or any byte — fully re-arms the 300s window.

Conclusion: idle-but-alive clients that send periodic NULLs are NOT wrongly
dropped. The worst-case concern from the config-defaults hunt does not
materialise **as long as the client emits traffic**. See Finding 2 for the
residual risk (server emits nothing of its own).

---

## Finding 2 — No server-initiated keepalive; 300s deadline can drop a genuinely quiet client  (MEDIUM — BUG / NOT-IMPLEMENTED)

Classification: BUG (behavioral divergence) — the liveness mechanism the
original relies on is not implemented in v3, while v3 adds a shorter hard
read-deadline.

### Original behaviour
The original has **no read-idle deadline** at all. Liveness is maintained two ways:

1. Server→client **SERVER_ECHOREQ** on a timer for bnet clients. Armed when the
   class becomes `conn_class_bnet`:
   `src/bnetd/connection.cpp:799-802`
   ```cpp
   delta = prefs_get_latency();
   data.n = delta;
   if (timerlist_add_timer(c, now + (std::time_t)delta, conn_test_latency, data) < 0) ...
   ```
   `conn_test_latency` (`connection.cpp:240-261`) pushes a `SERVER_ECHOREQ` and
   re-arms itself. Default interval `BNETD_LATENCY = 600` s
   (`src/common/setup_before.h:211`). The client's ECHOREPLY produces inbound
   traffic.
2. Server→client **SID_NULL** (`conn_send_nullmsg`, `connection.cpp:265-279`,
   interval `nullmsg`, default 120s, `setup_before.h:213`) — but ONLY for
   `conn_class_bot` (`connection.cpp:818-823`), **not** bnet clients.

Dead connections are reaped not by a read timeout but by a failed write / TCP
error (`connlist_reap`, `server.cpp:1544-1545`) and by the init-phase
`initkill_timer` (Finding 6).

### v3 behaviour
- Single absolute idle-read deadline of **300 s** for bnet
  (`src/app/bnetd/src/main.cpp:399`, `NetTimeoutsConfig::bnet = 300`,
  `src/infra/config/include/infra/config/server_config.hpp:232`).
- **No** `SERVER_ECHOREQ` timer. Grep for `ECHOREQ` / `conn_test_latency` /
  `nullmsg` across `src/app/bnetd/src` and `src/application/connection` returns
  nothing.
- **No** periodic `SID_NULL` sender. The `nullmsg = 120` value is parsed into
  `server_config` (`server_config.cpp:224`) and exposed via
  `legacy_prefs.hpp:192` but is never consumed by any sender.
- The bnet adapter (`src/app/bnetd/src/bnet_connection_adapter.cpp`, 99 lines)
  has no timer/keepalive/ping logic.

### Divergence / impact
The original never drops a quiet-but-connected bnet client on a read deadline,
and its only periodic server→client probe runs at 600s. v3 imposes a 300s read
deadline AND emits nothing server-side to provoke a reply. So a BNCS client that
is connected but silent for 300s (e.g. sitting at a menu, a client whose
self-keepalive interval exceeds 300s, or one expecting the server's NULL/echo to
prompt it) is closed with `boost::asio::error::timed_out`. The original would
keep it alive indefinitely (modulo the 600s echo, which it answers).

Real retail BNCS clients do send their own periodic SID_NULL, but the interval
is client/version dependent and is not guaranteed < 300s for every client/idle
screen; the original tolerated this, v3 does not. Note also: even if v3 later
added the original's 600s echo timer, 600 > 300 means the read deadline would
still fire first — the two values are mutually inconsistent.

### Proposed fix
Pick one (in order of fidelity to original):
1. Implement a server-side periodic `SID_NULL` (and/or `SERVER_ECHOREQ`) sender
   on a `steady_timer` at `nullmsg` (120s) for bnet/loggedin connections; this
   both keeps NAT/middleboxes warm and produces inbound replies that re-arm the
   read deadline. This restores original semantics and wires up the already-parsed
   `nullmsg` pref. Easiest place: the bnet adapter or session, reusing the
   existing `TcpSession::send`.
2. If keeping a read deadline, raise the bnet default well above the client's
   self-keepalive interval and above the original 600s echo (e.g. >= 660s), so
   the deadline only catches truly dead sockets, matching original intent.
3. Minimum: document that `[net.timeouts].bnet` MUST exceed the client keepalive
   interval and ship a larger default; do not leave it at 300 with no server
   keepalive.

---

## Finding 3 — Latency/ping measurement not implemented; `latency` never set  (LOW-MEDIUM — NOT-IMPLEMENTED)

Original measures round-trip latency via the SERVER_ECHOREQ / CLIENT_ECHOREPLY
(0x25ff) pair:
- send: `connection.cpp:248` `packet_set_type(packet, SERVER_ECHOREQ)` with
  `get_ticks()`.
- receive: `handle_bnet.cpp:984-1000` `_client_echoreply` computes
  `conn_set_latency(c, now - then)`.
This latency is surfaced in channel user lists (`channel_update_latency`,
`connection.cpp:1660`) and `/whois`/`/users` style output.

v3 implements **client-initiated** SID_PING echo only
(`src/application/connection/src/connection_fsm.cpp:47-55`: read the 4-byte
cookie, echo it back) but never sends `SERVER_ECHOREQ`, so the server-measured
latency value is never populated. Effect: channel/userlist latency readouts will
be zero/absent. Cosmetic/feature-parity, not a crash.

Protocol note: original `SERVER_ECHOREQ`/`CLIENT_ECHOREPLY` are `0x25ff`
(`bnet_protocol.h:3434,3444`), which is a *different* packet from v3's
`sid::kPing = 0x25` (`connection_fsm.hpp:127`). v3's 0x25 echo matches the real
SID_PING client-ping semantics and is correct on its own; it just isn't the
latency-measurement path.

Proposed fix: if latency display matters, add the SERVER_ECHOREQ timer/handler
alongside the Finding-2 keepalive work and store the RTT on the connection
context.

---

## Finding 4 — Connection-level packet flood limit (`packet_limit`) has no v3 analogue  (LOW-MEDIUM — NOT-IMPLEMENTED)

Original: if a connection's outqueue exceeds `packet_limit`
(default `BNETD_PACKET_LIMIT = 1000`, `setup_before.h:210`), the connection is
destroyed as a suspected hack/flood:
`connection.cpp:2274-2278`
```cpp
if (prefs_get_packet_limit() && queue_get_length(...) > prefs_get_packet_limit()) {
    conn_set_state(c, conn_state_destroy);
    eventlog(..., "outqueue reached limit of {} packets (hack attempt?)", prefs_get_packet_limit());
}
```
(Note: original's `packet_limit` guards the **out**queue depth, a backpressure/abuse
guard, not an inbound packet rate.)

v3: no equivalent. `TcpSession::send` appends to an unbounded
`std::deque<std::vector<std::byte>>` write queue (`tcp_session.cpp:79-90`,
`write_q_` in the header) with no length cap and no drop/destroy on overflow.
Grep for `packet_limit` / `flood` / `throttle` across `src/app/bnetd/src`,
`src/application/connection/src`, `src/infra/net/src` finds nothing.

Impact: a misbehaving/slow client whose outbound queue grows unbounded (server
generating data faster than the client drains it) is never cut off and can
accumulate memory. Lower severity than Finding 2 because it requires an
adversarial/broken client.

Proposed fix: cap `write_q_` depth (or total queued bytes); on exceed, close the
session (mirror original: log + destroy). Wire the already-parsed `packet_limit`
pref.

---

## Finding 5 — SID_PING (0x25) client echo  (MATCHES)

v3 `connection_fsm.cpp:47-55` reads the 4-byte cookie and echoes it back under
`sid::kPing` in all non-Disconnecting states. SID_NULL (0x00) is a no-op
(`connection_fsm.cpp:43-44`), legal in every non-Disconnecting state. Both match
the original's "ignore NULL, reply to ping" behaviour. No issue.

---

## Finding 6 — `initkill_timer` (stale-init reaper) absent  (LOW — INTENTIONAL)

Original arms a `conn_shutdown` timer at accept time for bnet listeners so a
connection that never sends its magic byte is killed:
`server.cpp:384-391` (`prefs_get_initkill_timer()`; default 0 = disabled).

v3 has the `initkill_timer` field parsed (`server_config.cpp:228`,
`legacy_prefs.hpp:196`, default 0) but unused. This is acceptable: the
unconditional 300s idle-read deadline already reaps a stalled init connection
(it never sends bytes, so the deadline fires). Because the original default is 0
(disabled), v3 is actually *more* aggressive at reaping init stalls, not less.
Classification: INTENTIONAL / subsumed.

---

## Finding 7 — Chat `Quota` not wired into the live path  (LOW — UNSURE)

`src/domain/moderation/include/domain/moderation/quota.hpp` implements a
sliding-window chat message rate limiter (mirrors legacy `quota.conf`: N msgs
per window → mute). This is the analogue of the original chat-flood guard
(`command.cpp:556-569`, "sending commands too quickly"), NOT the connection-level
`packet_limit` of Finding 4.

The only references to `Quota` outside tests are in
`src/app/pvpgn-config/main.cpp` (config tooling). It does not appear to be
invoked from the bnet/chat command path in `src/app/bnetd` or
`src/application/connection`. So spam throttling/mute may be a no-op at runtime.
Marked UNSURE — needs confirmation by whoever owns the chat-command subsystem;
flagging here only because it is the closest thing to a flood control and is
adjacent to this subsystem.

---

## What matches (no action)

- Idle deadline reset-on-activity (Finding 1).
- SID_NULL no-op + SID_PING echo (Finding 5).
- Connection-state model: v3 `ConnectionState` FSM
  (`connection_fsm.hpp:180-187`) is a clean, well-formed reimplementation of the
  legacy `conn_set_state` transitions; the legacy→v3 mapping is documented in the
  header (`connection_fsm.hpp:151-179`) and looks faithful. No transition bug found.
- `conn_set_state(conn_state_destroy)` → dead-list reaping is replaced by Asio
  `deliver_close` / `on_close` lifecycle; equivalent.

## Key risk ordering
1. **Finding 2** (MEDIUM): no server keepalive + 300s read deadline → real
   clients can be dropped. Most likely to cause user-visible disconnects.
2. **Finding 4** (LOW-MED): unbounded write queue, no flood/backpressure kill.
3. **Finding 3** (LOW-MED): latency display dead.
