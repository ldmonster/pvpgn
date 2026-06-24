# Bug Hunt — Channel / Chat / Messages subsystem

ORIGINAL: `/home/cnupt/work/pvpgn-server`  (read-only)
v3:       `/home/cnupt/work/pvpgn`         (read-only this phase)

Scope: channel flags, EID chat-event composition, whisper/emote/talk,
user flags, channel join rules, kick/ban/op permissions.

---

## Reference tables (ground truth from ORIGINAL)

SID_CHATEVENT event-ids — `src/common/bnet_protocol.h:2486-2508`:

| name                 | value |
|----------------------|-------|
| ADDUSER / SHOWUSER   | 0x01  |
| JOIN                 | 0x02  |
| PART / LEAVE         | 0x03  |
| WHISPER              | 0x04  |
| TALK                 | 0x05  |
| BROADCAST            | 0x06  |
| CHANNEL              | 0x07  |
| USERFLAGS            | 0x09  |
| WHISPERACK           | 0x0a  |
| CHANNELFULL          | 0x0d  |
| CHANNELDOESNOTEXIST  | 0x0e  |
| CHANNELRESTRICTED    | 0x0f  |
| INFO                 | 0x12  |
| ERROR                | 0x13  |
| EMOTE                | 0x17  |

User/message flags (`MF_*`) — `bnet_protocol.h:2570-2594`:
BLIZZARD 0x01, GAVEL(operator) 0x02, VOICE 0x04, BNET(sysop) 0x08,
PLUG 0x10, X(squelch) 0x20, SHADES 0x40.

Channel wire flags (`CF_*`) — `bnet_protocol.h:2599-2604`:
PUBLIC 0x01, MODERATED 0x02, RESTRICTED 0x04, THEVOID 0x08,
SYSTEM 0x20 (gap at 0x10!), OFFICIAL 0x1000.

---

## FINDING 1 — Wrong EID literals hand-coded in the live FSM chat path  [CRITICAL / BUG]

**v3 ref:** `src/protocol/bnet/src/fsm/fsm_chat.cpp`
**original ref:** `src/bnetd/message.cpp:984-1348` (`message_bnet_format`,
the canonical `SERVER_MESSAGE_TYPE_*` switch).

`fsm_chat.cpp` builds `ChatEvent{}` structs with hand-typed `event_id`
literals whose comments name the right EID but whose values are wrong.
The codec writes `m.event_id` verbatim
(`src/protocol/bnet/src/codec/codec_chat.cpp:127-128`,
`w.write_le<std::uint32_t>(m.event_id)`) — no remap — so the wrong
numbers go straight onto the wire. The application layer DOES have the
correct constants (`protocol/bnet/.../chat_wire_types.hpp:49-63`,
used by `chat_event_compose.cpp`), but the FSM bypasses
`compose_chat_event` and re-invents them incorrectly.

Concrete defects:

| fsm_chat.cpp line(s)              | comment    | emitted | correct | actually means |
|----------------------------------|------------|---------|---------|----------------|
| 63, 70, 154                      | EID_CHANNEL| **3**   | 7       | 3 = PART/LEAVE |
| 102, 233, 261, 276               | EID_INFO   | **4**   | 0x12=18 | 4 = WHISPER    |
| 170                              | EID_JOIN   | **1**   | 2       | 1 = SHOWUSER   |
| 416                              | EID_LEAVE  | **4**   | 3       | 4 = WHISPER    |
| 139 (EID_SHOWUSER 0x01)          | —          | 1       | 1       | OK             |
| 247, 290 (EID_TALK 5)            | —          | 5       | 5       | OK             |

Effect on a real client:
- Joining a channel: the "you are now in channel X" event is sent as
  EID_PART (3) instead of EID_CHANNEL (7) → client never updates its
  current-channel label.
- Every server INFO line (`/help`, error replies, "Channel is full",
  etc.) is sent as EID_WHISPER (4) → shown as a whisper from
  "Battle.net"/"" instead of a channel info line.
- Other members see a joiner as EID_SHOWUSER (1) instead of EID_JOIN
  (2) → no "X has joined" line.
- A leave is broadcast as EID_WHISPER (4) instead of EID_LEAVE (3) →
  member never removed from the userlist.

**Proposed fix:** route all of these through `compose_chat_event`
(which already has the right values) OR replace the literals with the
`pbc::kServerMessageType*` constants: CHANNEL→7, INFO→0x12, JOIN→2,
LEAVE/PART→3. (SHOWUSER and TALK are already correct.)

**Matches:** the application-layer `chat_event_compose.cpp` EID values
are 100% correct against original — this bug is confined to the FSM
shortcut.

---

## FINDING 2 — kick / ban require only channel membership, not operator  [HIGH / BUG]

**v3 ref:** `src/application/chat/src/kick_from_channel.cpp:24-38`,
`src/application/chat/src/ban_from_channel.cpp:24-39`, and the domain
rule `src/domain/chat/include/domain/chat/channel.hpp:154-160`
(`Channel::kick` only checks `contains(moderator)`).
**original ref:** `src/bnetd/command.cpp:2505-2536` (`_handle_kick_command`).

Original `/kick` (and `/ban`) reject the actor unless they are
admin / channel-operator / tmpOP:

```cpp
if (account_get_auth_admin(acc,NULL)!=1 && account_get_auth_admin(acc,chan)!=1 &&
    account_get_auth_operator(acc,NULL)!=1 && account_get_auth_operator(acc,chan)!=1 &&
    !channel_conn_is_tmpOP(channel, account_get_conn(acc)))
{ "You have to be at least a Channel Operator or tempOP..."; return -1; }
```
…and additionally refuse to kick admins/operators (lines 2525-2536).

v3 only checks `channel.contains(actor)` and a self-kick guard. Any
ordinary member can kick/ban any other member, including operators and
admins. This is a privilege-escalation divergence.

Note `op_from_channel.cpp:14` DOES gate on
`permissions_->has_command_group(...,"operator")`, so the seam exists;
kick/ban simply don't use it.

**Proposed fix:** add an operator/admin permission check (via the same
`IPermissionChecker`/command-group port used by `OpFromChannel`) to
`KickFromChannel`/`BanFromChannel`, plus the "cannot kick op/admin"
target guard.

---

## FINDING 3 — Self-whisper is rejected; original allows it  [MEDIUM / BUG]

**v3 ref:** `src/application/chat/src/whisper_use_case.cpp:35`
(`if (ieq(req.sender, req.target)) return WhisperVerdict::SelfWhisper;`).
**original ref:** `src/bnetd/command.cpp:169-219` (`do_whisper`).

Original `do_whisper` performs NO self-whisper check. The order is:
mute → not-logged-on → DND → ack+away+deliver. Whispering yourself is
allowed in original PvPGN (you receive your own whisper). v3
`decide_whisper` adds a `SelfWhisper` rejection that has no counterpart
in the original, changing observable behaviour.

The task brief flags "wrong 'you cannot whisper yourself' priority" —
here the correct priority is *no such rule at all* for the bnet path.

**Proposed fix:** remove the `SelfWhisper` arm (or gate it behind a
config flag) to match original. If kept, document it as INTENTIONAL.

**Sub-note (MEDIUM / BUG):** the two whisper implementations disagree
with each other. The DI use-case actually wired into the service,
`src/application/chat/src/send_whisper.cpp:12-44`, checks only
empty-body / name-parse / target-online and does NOT call
`decide_whisper` at all — so the pure decision module (with its
self/dnd/ignore rules) is dead for the live path. Whichever is "the"
whisper path should be the one exercised.

---

## FINDING 4 — Whisper priority: offline vs DND ordering, and missing "away"  [LOW / UNSURE]

**v3 ref:** `whisper_use_case.cpp:33-39`.
**original ref:** `command.cpp:180-207`.

Original order: not-logged-on (offline) → DND(block) → deliver, and
"away" does NOT block — it sends an INFO line *and still delivers* the
whisper (lines 201-207). v3 `decide_whisper` checks
SelfWhisper → TargetOffline → TargetDnd → IgnoredByTarget and has no
"away" concept at all, so an away target's whisper is `Delivered` but
the "<user> is away" notice is never produced. Delivery itself matches;
the missing away-notice is a feature gap. `IgnoredBySender` /
`IgnoredByTarget` enum arms also have no original equivalent in
`do_whisper` (squelch is handled via `MF_X` at compose time, see
Finding 7). Classifying UNSURE pending the intended whisper path.

---

## FINDING 5 — Channel auto-create on join uses default flags/max; minor name-rule mismatch  [LOW / UNSURE]

**v3 ref:** `src/application/chat/src/join_channel.cpp:18-40`.
**original ref:** `src/bnetd/channel.cpp:68-244`, conf-driven creation.

(a) Name validation in `JoinChannel` only rejects empty / NUL / `\x01`
(`join_channel.cpp:18-20`), NOT the stricter `ChannelName::parse`
(printable ASCII 0x20-0x7E, len 1-64) rule that exists in
`domain/chat/.../channel_name.hpp`. The aggregate's own name rule is
bypassed on the join path — inconsistent, low impact.

(b) Auto-created channels get `flags = {}` (all clear) and
`max_members = 0` (unlimited). Original treats `maxmembers == -1` as
unlimited (`channel.cpp:1489`); v3 uses `0` as its unlimited sentinel
(`channel.hpp:55-56,88-90`). Internal sentinel only — not a wire bug,
but be careful if `0` ever leaks to a numeric "max" field.

(c) Original channel lookup is case-insensitive (`strcasecmp`,
`channel.cpp:124,794-833`); v3 `ChannelName::operator==`/`<=>` are
case-sensitive `=default` and `JoinChannel` uses `find_by_name` raw,
so "Foo" and "foo" may resolve to different channels depending on the
repo. UNSURE — depends on repository key normalization.

---

## FINDING 6 — No `cflags_to_bncflags` equivalent; ChannelFlag bit positions ≠ CF_ wire bits  [MEDIUM / UNSURE→BUG-risk]

**v3 ref:** `domain/chat/.../channel.hpp:28-37` (`enum class ChannelFlag`,
bit *positions* 0..7 in a `std::bitset<8>`), surfaced as
`JoinChannelResult.flags` (`join_channel.cpp:97`) and
`ComposeRequest.channel_flags_bncflags` (`chat_event_compose.cpp:108`).
**original ref:** `src/bnetd/channel_conv.cpp:30-49` (`cflags_to_bncflags`)
mapping internal `channel_flags_*` to wire `CF_*`.

Original never sends the internal flag bitfield raw — it always passes
the channel flags through `cflags_to_bncflags`, which maps to the
*non-contiguous* CF_ wire values (PUBLIC 0x01, MODERATED 0x02,
RESTRICTED 0x04, THEVOID 0x08, SYSTEM **0x20**, OFFICIAL **0x1000**).

In v3 there is NO such conversion anywhere
(grep for `cflags_to_bncflags` / `CF_PUBLIC` in v3 finds none). The
`ChannelFlag` enum uses dense positions
(Public=0, Permanent=1, Moderated=2, Restricted=3, Silent=4, System=5,
AllowBots=6, Locked=7) and `ChannelFlags` is a `std::bitset<8>`. If a
`ChannelFlags` value is ever serialized to the wire as-is for the
EID_CHANNEL `flags` field, the bits will be wrong: e.g. Moderated is
bit 2 (value 0x04) in v3 but CF_MODERATED is 0x02 on the wire;
System is bit 5 (0x20) which coincidentally equals CF_SYSTEM, etc.
Currently the live FSM hardcodes `flags=0` for EID_CHANNEL
(`fsm_chat.cpp:155`) so the bug is latent, but the moment channel
flags are wired through, the mapping table is missing.

**Proposed fix:** add an explicit `ChannelFlag → CF_*` conversion
(mirroring `cflags_to_bncflags`, including the 0x10 gap and
OFFICIAL=0x1000) and use it wherever the EID_CHANNEL `flags` field is
populated. Also reconcile the v3 enum's `Public/Permanent/AllowBots/
Locked` vs original's `public/permanent/allowbots/allowopers/clan/
autoname/thevoid/official` — several original flags have no v3 analog
and vice-versa.

---

## FINDING 7 — EID_USERFLAGS / user-flag bits: composition matches, but FSM never emits them  [LOW / INFO]

**original ref:** `message.cpp:1187-1211` (USERFLAGS uses
`conn_get_flags(me) | dstflags` as the flag field, username =
chatcharname, text = playerinfo).
**v3 ref:** `chat_event_compose.cpp:114-121` — same shape
(flags = me_flags|dstflags, username = chatcharname, text = playerinfo,
EID = 0x09). This MATCHES.

However, the FSM (`fsm_chat.cpp`) never produces a USERFLAGS event at
all, so operator/voice (MF_GAVEL 0x02 / MF_VOICE 0x04) promotions are
not pushed to clients. `OpFromChannel` (`op_from_channel.cpp:43-58`)
explicitly does NOT update any flag or emit USERFLAGS — it's a
structural no-op with a `(void)cmd.grant;`. So op/voice status is never
reflected on the wire. Feature gap, not a wrong-bit bug.

---

## What MATCHES (verified correct against original)

- `compose_chat_event` EID values for ALL 16 message types vs
  `SERVER_MESSAGE_TYPE_*` (chat_wire_types.hpp:49-63). ✓
- Per-EID username/text field assignment in `compose_chat_event`
  mirrors `message_bnet_format` exactly:
  - PART → username=chatcharname, text="" (msg.cpp:1075-1082 vs compose:62-63) ✓
  - CHANNEL → flags=channelflags, username=chatname (NOT chatcharname),
    text=text (msg.cpp:1168-1185 vs compose:107-111) ✓
  - USERFLAGS/ADDUSER/JOIN → text=playerinfo (msg.cpp vs compose) ✓
  - INFO/ERROR → flags=0, username="" (msg.cpp:1293-1316 vs compose:167-181) ✓
  - CHANNELFULL/RESTRICTED → flags=0,user="",text="" ✓
  - FriendWhisperAck → literal username "your friends",
    EID=WHISPERACK (msg.cpp:1235-1253 vs compose:132-140) ✓
  - WHISPER me==NULL → username=servername, flags=dstflags, ping=0
    (msg.cpp:1095-1107 vs compose:66-79) ✓
- `compose_chat_event` MF_X rejection on whisper/talk/broadcast/emote
  (`dstflags_mf_x`) mirrors original `if (dstflags&MF_X) return -1;`. ✓
- `acct_number`/`registration` magic 0xBAADF00D and dummy IP mirror
  legacy `SERVER_MESSAGE_*` dummies. ✓ (NB: the FSM uses a *different*
  magic 0xBADC0FFE in some hand-built events — cosmetic, both are
  ignored by clients, but inconsistent with compose's 0xBAADF00D.)
- `Channel::admit` rejection order (Locked → Banned → WrongClientTag →
  Full) is reasonable and `channel_check_banning` parity is preserved
  conceptually.

---

## Severity summary

| # | sev      | class   | one-liner |
|---|----------|---------|-----------|
| 1 | CRITICAL | BUG     | fsm_chat hardcodes wrong EID literals (CHANNEL=3, INFO=4, JOIN=1, LEAVE=4) — bypasses correct compose |
| 2 | HIGH     | BUG     | kick/ban need only channel membership, not operator/admin (priv-esc) |
| 3 | MEDIUM   | BUG     | self-whisper rejected; original allows it. Also two divergent whisper paths |
| 4 | LOW      | UNSURE  | whisper offline/dnd order + missing "away" notice |
| 5 | LOW      | UNSURE  | join auto-create bypasses ChannelName rule; case-sensitivity; max sentinel |
| 6 | MEDIUM   | UNSURE  | no ChannelFlag→CF_ wire mapping (latent: FSM sends flags=0 today) |
| 7 | LOW      | INFO    | USERFLAGS/op-voice never emitted; compose itself is correct |
