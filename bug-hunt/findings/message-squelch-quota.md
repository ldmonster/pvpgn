# Bug-Hunt: Messaging internals — squelch/ignore, routing/format, quota/flood, emote

Subsystem: messaging internals (squelch/ignore, message routing/format, idle/away/dnd,
flooding/anti-spam, timed events).

- ORIGINAL: `/home/cnupt/work/pvpgn-server`
- CURRENT v3: `/home/cnupt/work/pvpgn`

Method: read original `src/bnetd/message.cpp`, `connection.cpp` (quota), `command.cpp`
(squelch / whisper / dnd / away), and compared against v3
`src/application/chat/*`, `src/domain/chat/*`, `src/domain/moderation/quota.hpp`,
and the only live consumer `src/protocol/bnet/src/fsm/fsm_chat.cpp`.

Headline: the v3 domain layer contains *correct-looking* `IgnoreList`, `Quota`, and
`decide_whisper` building blocks, but **none of them are wired into the live message
path**. The live channel-talk path broadcasts to every member with no squelch filter
and no quota check. So the most severe items are NOT-IMPLEMENTED (live wiring absent)
even though unit-tested domain code exists.

---

## Finding 1 — Squelch/ignore NOT applied on the channel-talk path (spam/harassment vector)

- Severity: HIGH
- Classification: NOT-IMPLEMENTED (live wiring); the domain `IgnoreList` exists but is dead code.

Original ref — squelch is applied **per recipient** at send time, keyed off the
LISTENER's ignore list (correct direction):
`src/bnetd/message.cpp:1549`
```cpp
dstflags = 0;
if (message->src) {
    char const * tname;
    if ((tname = conn_get_chatname(message->src)) && conn_check_ignoring(dst, tname) == 1) {
        conn_unget_chatname(message->src, tname);
        dstflags |= MF_X;          // mark this dst as ignoring the speaker
    }
    ...
}
```
and the formatter drops TALK/EMOTE/WHISPER/BROADCAST when `dstflags & MF_X`
(`message.cpp:1092,1123,1143,1328`, etc.). i.e. each `message_send(message, dst)`
re-evaluates `dst`'s ignore list and suppresses the line only for that listener.

v3 ref — live path builds recipients = *all* members except sender, with no ignore
filter:
`src/application/chat/src/post_message.cpp:50`
```cpp
std::vector<domain::SessionId> recipients;
auto member_ids = channel.member_ids();
for (const auto& member_id : member_ids) {
    if (member_id.value() != account_id.value()) {
        if (auto sid = session_registry_.session_for(member_id))
            recipients.push_back(sid.value());     // no squelch check
    }
}
```
and the FSM broadcasts to that list verbatim:
`src/protocol/bnet/src/fsm/fsm_chat.cpp:301-313` (`broadcast_chat_event(... post_result.value().recipients)`).

The `IgnoreList` aggregate (`src/domain/chat/include/domain/chat/whisper.hpp:48`,
with a correct `ignores(AccountId)` query) is **never referenced** in any non-test
production source (`grep -rn "IgnoreList\|\.ignores(" src` outside tests/headers: 0 hits).
There is no `IgnoreRepository` lookup in `PostMessage`/`SendEmote`.

Divergence: a squelched user's channel talk/emote is still delivered to the squelcher.
Direction was correct in original (listener's list); v3 simply omits the check.

Proposed fix: inject an ignore-list lookup (per-recipient) into `PostMessage::execute`
(and `SendEmote`). For each candidate `member_id`, load that member's `IgnoreList`
and skip the recipient if `il.ignores(sender_account_id)` — mirroring
`conn_check_ignoring(dst, speaker)`. Do NOT key off the speaker's list.

---

## Finding 2 — Flood/quota throttle NOT applied on any live message path (spam vector)

- Severity: HIGH
- Classification: NOT-IMPLEMENTED (live wiring); domain `Quota` exists but is dead code.

Original ref — every talk/emote/broadcast goes through `conn_quota_exceeded` first:
`src/bnetd/handle_bnet.cpp:3735` (`else if (channel && !conn_quota_exceeded(c, text))`),
also `command.cpp:567,1842`, `handle_telnet.cpp:339`, `handle_bot.cpp:341`,
`message.cpp:1681`. Implementation `src/bnetd/connection.cpp:3101`.

v3 ref — the live talk path (`fsm_chat.cpp:286` → `PostMessage::execute`) performs
**no** quota check. `grep -rn "\.record(" src` outside tests: 0 hits — the
`moderation::Quota` rate limiter (`src/domain/moderation/quota.hpp:37`) is never
invoked in production. `Channel::post` (`channel.hpp:146`) unconditionally accepts
once membership is verified.

Divergence: a client can flood a channel without throttle, warning, mute, or dobae
disconnect.

Proposed fix: wire a per-account `Quota` (or equivalent) check into the talk/emote/
broadcast use-cases before `Channel::post`; on `Throttled`/`Muted` emit the error
message and drop the line (and disconnect on the dobae threshold). See Finding 3 for
the semantic gaps that must also be fixed when wiring.

---

## Finding 3 — `moderation::Quota` semantics diverge from `conn_quota_exceeded`

- Severity: MEDIUM (becomes HIGH once Finding 2 is wired, because the limiter is weaker than legacy)
- Classification: BUG (implemented-but-wrong, relative to legacy contract the header claims to mirror)

The header claims to "Mirror legacy `quota.conf` semantics" (`quota.hpp:6`) but omits
several behaviours of `conn_quota_exceeded` (`connection.cpp:3101-3161`):

1. Line-length cap (`quota_maxline`). Original: `connection.cpp:3113`
   ```cpp
   if (std::strlen(text) > prefs_get_quota_maxline()) {
       message_send_text(con, message_type_error, con, "Your line length quota has been exceeded!");
       return 1;                       // line rejected outright
   }
   ```
   v3 `Quota` has no max-line concept at all. (See also Finding 5 on truncation.)

2. Long-line cost via `quota_wrapline`. Original charges a long message as multiple
   "lines": `connection.cpp:3137`
   ```cpp
   if (std::strlen(text) > prefs_get_quota_wrapline())
       qline->count = (strlen(text) + prefs_get_quota_wrapline() - 1) / prefs_get_quota_wrapline();
   else
       qline->count = 1;
   ```
   v3 `Quota::record` (`quota.hpp:46`) pushes exactly one timestamp per message —
   a 4000-char message costs 1, not N. Weakens flood protection.

3. Dobae kick threshold (`quota_dobae`). Original disconnects abusers:
   `connection.cpp:3149-3156` (`conn_set_state(con, conn_state_destroy)` + DOBAE log).
   v3 `QuotaPolicy` has only `limit`/`window`/`mute_for`; no disconnect tier.

4. Threshold comparison is off-by-one vs legacy intent. Original warns when
   `totcount >= quota_lines` (`connection.cpp:3146`, `>=`). v3 throttles only when
   `timestamps_.size() > policy_.limit` (`quota.hpp:47`, strict `>`), i.e. it allows
   `limit + 1` messages in-window before muting, whereas legacy mutes at exactly
   `quota_lines` line-units.

5. Defaults are hard-coded and unrelated to `quota.conf` (`limit=5`, `window=2s`,
   `mute_for=30s` — `quota.hpp:22-24`). Legacy reads `quota_lines/quota_time/
   quota_wrapline/quota_maxline/quota_dobae` from prefs
   (`prefs.cpp:105-110`). These should be sourced from config, not literals.

Proposed fix: when wiring (Finding 2), build `QuotaPolicy` from the legacy prefs,
add max-line rejection + wrapline cost accounting + dobae disconnect tier, and use
`>=` for the warn threshold to match `conn_quota_exceeded`.

---

## Finding 4 — Whisper executor ignores dnd/away/squelch and never delivers

- Severity: HIGH
- Classification: BUG + NOT-IMPLEMENTED (executor is a stub; the correct decision fn is bypassed)

Original ref — `do_whisper` (`src/bnetd/command.cpp:169`):
- mute check (`account_get_auth_mute`, line 174),
- DND short-circuit: if target has `conn_get_dndstr`, reply "X is unavailable (...)"
  and do NOT deliver (line 192-197),
- send `message_type_whisperack` to sender (line 199),
- AWAY: deliver but also inform sender "X is away (...)" (line 201-205),
- deliver `message_type_whisper` to target (line 207),
- set lastsender for `/reply` (line 209-218).

v3 ref — the wired executor `src/application/chat/src/send_whisper.cpp:11` looks up
the target, checks online, then **returns success without routing anything**:
```cpp
// 4. Route the whisper via the message router ...
(void)router_;          // <-- nothing is sent
return {};
```
It never consults dnd/away, never checks the recipient's ignore list, never sends a
whisperack, and never actually delivers the message (the router is explicitly cast to
void). Note: a correct pure decision function `decide_whisper`
(`src/application/chat/src/whisper_use_case.cpp:32`) DOES exist and handles
`TargetDnd` / `IgnoredByTarget` / `SelfWhisper` / `EmptyBody` — but `send_whisper.cpp`
does not call it, and `grep -rn decide_whisper src` shows no production wiring beyond
the header/impl pair.

Divergence: whispers are effectively no-ops in the v3 application layer; dnd/away/
ignore/whisperack/lastsender are all unimplemented on the live path.

Proposed fix: route `send_whisper` through `decide_whisper`, then on `Delivered`
emit whisperack to sender + whisper to target via `router_`; on `TargetDnd`/away/
`IgnoredByTarget` emit the corresponding info/silent-drop exactly as `do_whisper` does.

---

## Finding 5 — No max message-length cap / truncation on the v3 talk path

- Severity: MEDIUM
- Classification: NOT-IMPLEMENTED

Original caps line length two ways: the maxline quota rejection
(`connection.cpp:3113`, see Finding 3.1) and protocol-level read limits. v3
`ChatMessage::create` is the only gate (`fsm_chat.cpp:273`); confirm whether it bounds
length. The talk path forwards `m.text` verbatim into the broadcast ChatEvent
(`fsm_chat.cpp:312`) with no truncation/length guard equivalent to `quota_maxline`.

Proposed fix: enforce a max length in `ChatMessage::create` (or the quota wiring) and
reject/truncate over-long lines as legacy does.

---

## Finding 6 — Emote (/me) event id field mapping: MATCHES (with one wiring caveat)

- Severity: LOW
- Classification: MATCHES (compose) / UNSURE (live wiring)

The compose layer maps Emote correctly:
`src/application/chat/src/chat_event_compose.cpp:183`
```cpp
case LegacyMessageType::Emote:
    if (!r.me_present)   return fail_not_found("emote: me==NULL");
    if (r.dstflags_mf_x) return fail_not_found("emote: MF_X");   // squelch honoured here
    ev.event_id = pbc::kServerMessageTypeEmote;                  // EID_EMOTE
    ev.flags    = me_flags_combined;
    ev.ping_ms  = r.me_latency;
    ev.username = std::string(r.chatcharname);                   // speaker name
    ev.text     = std::string(r.text);                          // emote text
    return ev;
```
This matches the original `message_bnet_format` EID_EMOTE arm (username = chatcharname,
text = body, suppressed on `dstflags & MF_X`). Good: this compose path *does* honour
MF_X.

Caveat: `SendEmote::execute` (`src/application/chat/src/send_emote.cpp:37-39`) returns
`recipients` as an empty vector with a comment "would look up session IDs ... For now,
return empty" — so the emote use-case currently routes to nobody, and (like Finding 1)
applies no per-recipient squelch when it eventually does. So compose is correct but the
live emote routing is incomplete. Confirm whether the live `/me` path uses
`chat_event_compose` (which honours MF_X) or `SendEmote` (which does not filter).

---

## Finding 7 — `PostMessage` builds an unused event + dead loop

- Severity: LOW (correctness smell, not a security bug)
- Classification: BUG (minor)

`src/application/chat/src/post_message.cpp:39-45`: `drain_events()` is called and the
events iterated in a loop that does nothing (`(void)ev;`), while a fresh
`ChannelMessageSent msg_event` is constructed separately and returned. The drained
domain events are discarded, so any consumer relying on domain events from a posted
message won't see them. Harmless today but will cause an event-sourcing/audit gap if
events are later consumed.

Proposed fix: either propagate the drained events or remove the dead drain/loop and
keep a single source of truth for the emitted event.

---

## What MATCHES / is correct

- `IgnoreList` (domain) models the listener-side ignore set with `ignores(target)`,
  correct direction; `add/remove` are idempotent and emit `IgnoreAdded/Removed`.
  Only problem is it is unused in production (Finding 1).
- `decide_whisper` correctly encodes self-whisper, empty-body, offline, dnd, and
  ignored-by-target precedence (`whisper_use_case.cpp:32-40`). Only problem: the live
  executor bypasses it (Finding 4).
- `chat_event_compose` honours `MF_X` for Whisper/Talk/Broadcast/Emote
  (`chat_event_compose.cpp:67,83,97,185`) and mirrors the legacy NULL-`me` contracts
  including the deliberate broadcast me==NULL guard and the "your friends" /
  "Battle.net" username literals.
- `Quota` window/eviction logic is structurally sound (sliding deque + mute window);
  the divergences are the missing legacy features in Finding 3.

---

## Summary of classifications

| # | Item | Severity | Class |
|---|------|----------|-------|
| 1 | Squelch not applied on channel talk/emote | HIGH | NOT-IMPLEMENTED (domain code dead) |
| 2 | Quota/flood not applied on any live path | HIGH | NOT-IMPLEMENTED (domain code dead) |
| 3 | Quota semantics weaker than legacy (maxline/wrapline/dobae/`>=`/config) | MED→HIGH | BUG |
| 4 | Whisper executor stub: no dnd/away/squelch/ack/delivery | HIGH | BUG + NOT-IMPLEMENTED |
| 5 | No max-line cap/truncation on talk path | MED | NOT-IMPLEMENTED |
| 6 | Emote compose mapping | LOW | MATCHES (live wiring UNSURE) |
| 7 | PostMessage discards drained events | LOW | BUG (minor) |

## Wave 54: squelch is per-connection (cleared on disconnect)
Verified differentially (tests/diff/diff_squelch_reconnect.py): the oracle clears
a user's ignore list when their connection is destroyed; a reconnect starts empty.
v3's account-keyed InMemoryIgnoreStore persisted the squelch across reconnect.
Fixed with IIgnoreStore::clear_owner(account) called from BnetFsm::on_disconnect
(alongside the channel- and game-leave cleanup). Safe because kick-old-login
keeps a single live session per account.
