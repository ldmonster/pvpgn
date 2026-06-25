# Channel routing / auto-join / default-channel — ORIGINAL vs v3

Subsystem: channel selection, auto-join, default channel, permanent channels,
channel naming per clienttag, overflow split.

- ORIGINAL: `/home/cnupt/work/pvpgn-server`
- v3:       `/home/cnupt/work/pvpgn`

Scope note: in BNCS the *default channel name* a client lands in on first join is
chosen client-side (the client sends `SID_JOINCHANNEL` with `CLIENT_JOINCHANNEL_GENERIC`
or `_CREATE` and the default name like "Brood War USA-1"). The server's job is
(a) honour the join flag (esp. W3 GENERIC → clan channel, ask_new_channel for NORMAL),
(b) normalize/lookup the name case-insensitively, (c) roll a full permanent channel
over to a numbered sibling, (d) seed permanent/moderated/per-tag channels from config.
The findings below are organised around those responsibilities.

---

## F1 — JOINCHANNEL flag (NORMAL/GENERIC/CREATE) completely ignored
**Severity:** HIGH
**Classification:** NOT-IMPLEMENTED (flag is parsed but discarded)

**Original ref:** `src/bnetd/handle_bnet.cpp:3662-3696` (`_client_joinchannel`)
```cpp
switch (bn_int_get(packet->u.client_joinchannel.channelflag)) {
case CLIENT_JOINCHANNEL_NORMAL:   // 0x00
    if (prefs_get_ask_new_channel() && (!channellist_find_channel_by_name(cname,...)))
        { found = 0; message_send_text(c, message_type_channeldoesnotexist, c, cname); }
    break;
case CLIENT_JOINCHANNEL_GENERIC:  // 0x01  -> first-join / default channel
    if ((user_clan = account_get_clan(account)) && (clantag = clan_get_clantag(user_clan)))
        { cname = ("Clan " + clantag_to_str(clantag)); }   // redirect to clan channel
    break;
case CLIENT_JOINCHANNEL_CREATE:   // 0x02
    break;
}
```
Flag constants: `src/common/bnet_protocol.h:2314-2316`
(`NORMAL=0x00`, `GENERIC=0x01`, `CREATE=0x02`).

**v3 ref:**
- `src/protocol/bnet/include/protocol/bnet/messages/messages_chat.hpp:18-22` — `JoinChannel{ uint32 flags; string channel; }`
- `src/protocol/bnet/src/codec/codec_chat.cpp:11-13` — `RD_U32(m.flags)` decoded …
- `src/protocol/bnet/src/fsm/fsm_chat.cpp:70-196` — `BnetFsm::on(JoinChannel&)` never reads `m.flags`.
- `src/application/connection/src/connection_fsm_inchannel.cpp:39-43` — comment documents the flags but `read_cstring(payload,4)` simply skips the 4-byte flag word; value is never inspected.

**Divergence:**
1. **GENERIC (first-join) clan redirect is gone.** A W3/W3XP user in a clan, on the
   "generic" first-join, should be routed to `Clan XXXX`, not to the literal name the
   client sent. v3 joins whatever name arrived. Clan members no longer auto-land in
   their clan channel.
2. **`ask_new_channel` semantics are gone.** Original, on NORMAL join with
   `ask_new_channel=true` (the shipped default, `conf/bnetd.conf.in:283`), refuses to
   silently create a brand-new channel and instead sends `channeldoesnotexist`. v3
   *always* creates the channel on miss (`join_channel.cpp:26-40`), so any typo spawns a
   permanent-less temp channel. Opposite of stock behaviour.

**Proposed fix:** In the bnet FSM `on(JoinChannel)`, branch on `m.flags`:
GENERIC → if account has a clan, rewrite target to `"Clan "+clantag`; NORMAL → if the
"ask_new_channel" policy is on and `find_by_name` misses, reply channel-does-not-exist
instead of creating; CREATE → allow create. Thread the flag through the connection-FSM
handler too (don't discard the 4 bytes).

---

## F2 — Channel overflow split into numbered siblings ("...-2") not implemented
**Severity:** HIGH
**Classification:** NOT-IMPLEMENTED

**Original ref:** `src/bnetd/channel.cpp:1435-1557` (`channellist_find_channel_by_name`)
and `channel_format_name` (`channel.cpp:1127-1153`). When a permanent channel matched
by short-name is full (`channel->currmembers >= channel->maxmembers`), the original
counts existing rollover siblings (`maxchannel`) and creates a new permanent copy
`"<short> <country>-<n+1>"` (e.g. "Brood War USA-2"), preserving tag/bot/oper/log/
moderated/maxmembers flags:
```cpp
if (channel->maxmembers == -1 || channel->currmembers < channel->maxmembers)
    return channel;                       // room here
...
maxchannel++;                             // count full siblings
...
if (foundperm) {                          // all full -> spawn next numbered copy
    channelname = channel_format_name(saveshortname, savecountry, saverealmname, maxchannel+1);
    channel = channel_create(channelname, saveshortname, savetag, 1, savebotflag, ...);
}
```

**v3 ref:** `src/application/chat/src/join_channel.cpp:24-54`,
`src/domain/chat/include/domain/chat/channel.hpp:88-90,126-130`. On `is_full()` the
domain returns `JoinOutcome::Full` and the use-case returns `JoinChannelError::Full`;
the client just sees "Channel is full". No sibling lookup, no `-N` formatting, no
`channel_format_name` equivalent exists anywhere in the tree (grep for
`format_name`/`rollover`/`-%u` returns nothing in domain/application/infra).

**Divergence:** Public flagship channels (Brood War USA-1, etc.) with a maxusers cap
are hard-capped instead of rolling over to USA-2/USA-3. With `maxusers_per_channel`
set, users get bounced rather than redistributed.

**Proposed fix:** Add a channel-routing service (application layer) that, on `Full`
for a permanent/system channel, finds-or-creates the next numbered sibling using a
`format_name(shortname, country, realm, n)` helper mirroring `channel_format_name`,
and retries the admit. Needs the channel's short-name + country/realm metadata
(see F4) to reconstruct the family.

---

## F3 — No default-channel auto-join on ENTERCHAT (per clienttag)
**Severity:** MEDIUM
**Classification:** UNSURE / matches original *server* behaviour but missing the
GENERIC plumbing that makes it work end-to-end.

**Original ref:** `_client_enterchat` does NOT itself force a channel; the client sends
a follow-up `SID_JOINCHANNEL`/GENERIC with the per-product default name. So the server
side of ENTERCHAT not joining a channel is *correct*. (Original `conn_set_channel` does
the actual placement, `src/bnetd/connection.cpp:1877`.)

**v3 ref:**
- `src/protocol/bnet/src/fsm/fsm_chat.cpp:58-68` — `on(EnterChatRequest)` only echoes
  `EnterChatReply`; no channel join. **MATCHES** original.
- `src/application/connection/src/connection_fsm_loggedin.cpp:25-59` — same, just echoes
  reply. **MATCHES**.

**Divergence:** Behaviour itself matches *only if* the subsequent GENERIC join is
honoured. Because F1 drops the GENERIC clan-redirect and `ask_new_channel` handling, the
practical end-to-end "land in the right default channel" flow is still broken — but the
ENTERCHAT handler in isolation is fine. Listed so it is not mistaken for a separate bug.

**Proposed fix:** none for ENTERCHAT itself; fix is F1.

---

## F4 — channel.conf metadata (clienttag, moderated, country, realm, shortname) dropped
**Severity:** HIGH
**Classification:** NOT-IMPLEMENTED (partial loader) + BUG (semantics)

**Original ref:** `src/bnetd/channel.cpp:939-1119` (`channellist_load_permanent`)
parses all 10 columns: name, sname, **tag**, bot, oper, log, **country**, **realmname**,
max, **moderated**, and calls
`channel_create(name, sname, clienttag, perm, bot, oper, log, country, realm, max, mod, clan, autoname)`.
The tag drives per-product admission and message routing
(`channel_message_send`, `channel.cpp:707-749`), moderated drives talk-gating
(`channel.cpp:694-705`), country/realm drive localized rollover (F2),
shortname drives name lookup (`channel.cpp:1478`).

**v3 ref:** `src/infra/config/src/channel_config_loader.cpp:69-111`
```cpp
/*std::string cltag =*/ next_token(line, pos);   // col 2  DISCARDED
...
/*std::string ctry =*/ next_token(line, pos);    // col 6  DISCARDED
/*std::string realm =*/ next_token(line, pos);   // col 7  DISCARDED
out.name = (special == "NONE") ? shortname : special;
out.flags = 0x41;                                 // always Permanent|AllowBots
// col 9 (moderated) never read
```
Short name kept only as a name fallback; `cltag`, `country`, `realm`, `moderated`,
`bots`, `ops`, `log` are all thrown away.

**Divergence:**
- `policy.client` (the per-tag restriction) is therefore *always empty*, so the
  `WrongClientTag` admission path in the domain (`channel.hpp:121-125`) can never fire
  for config channels — Diablo/SC/W3 users all share the same "Diablo II" room with no
  tag isolation.
- `Moderated` flag never set from config → moderated channels become free-for-all.
- Country/realm absent → rollover families can't be reconstructed (compounds F2).

**Proposed fix:** Extend `ChannelConfigEntry` with `client_tag`, `country`, `realm`,
`shortname`, `moderated`; parse cols 2/6/7/9; map them onto `ChannelPolicy.client`,
`ChannelFlag::Moderated`, and retained metadata for rollover.

---

## F5 — channel.conf file is never loaded at runtime (only 5 hardcoded channels)
**Severity:** HIGH
**Classification:** NOT-IMPLEMENTED

**Original ref:** `src/bnetd/channel.cpp:1282-1287` (`channellist_create` →
`channellist_load_permanent(prefs_get_channelfile())`) reads the operator's
`channel.conf` at startup. The full stock file defines dozens of per-product permanent
channels.

**v3 ref:** `src/services/bnetd/src/bnetd_service.cpp:62-143`. Seeding uses a private
`kDefaultChannels[]` table of **5** names (`The Void`, `Starcraft USA-1`, `Diablo II`,
`Warcraft 3`, `Chat`), all `max_members = 0`, flags `Permanent|AllowBots`, **no tag**.
`ChannelConfigLoader::load(path)` exists but is referenced nowhere outside its own
header/impl — grep `ChannelConfigLoader` across the repo hits only
`channel_config_loader.{cpp,hpp}` and a comment in `bnetd_service.hpp:32`; the
`.load()` path and even `ChannelConfigLoader::defaults()` are dead in production.

**Divergence:** Operators editing `channel.conf` get no effect; only 5 fixed channels
exist, none tag-restricted. Permanent-channel customization is entirely lost.

**Proposed fix:** Wire `ChannelConfigLoader::load(prefs.channelfile())` (fall back to
`defaults()`) into `BnetdService` seeding, replacing the `kDefaultChannels[]` literal.
Combine with F4 so loaded metadata actually reaches `ChannelPolicy`.

---

## F6 — Channel name lookup is case-SENSITIVE (original is case-insensitive)
**Severity:** MEDIUM
**Classification:** BUG

**Original ref:** all channel name matching uses `strcasecmp`:
`channel.cpp:1422` (`channellist_find_channel_by_fullname`), `channel.cpp:1472,1478`
(`channellist_find_channel_by_name`), `connection.cpp:1915` (`strncasecmp("clan ",…)`).
"brood war usa-1" and "Brood War USA-1" resolve to the same channel.

**v3 ref:** `src/infra/persistence/channel_repository.cpp:43-46`
```cpp
"SELECT id, name, topic, flags, max_members FROM channels WHERE name = ?"
```
SQLite `=` on a `TEXT` column is **case-sensitive** by default (no `COLLATE NOCASE`).
The in-memory store path (`src/infra/inmemory/...channel_repository.hpp`) similarly keys
by exact string. `JoinChannel::execute` (`join_channel.cpp:24`) therefore treats a
differently-cased name as a miss and **creates a duplicate channel**.

**Divergence:** Users typing the channel name with different case land in a separate
(new) channel instead of the existing one → fragmented rooms; also defeats the
"already in this channel" short-circuit that exists in the original
(`handle_bnet.cpp:3657`).

**Proposed fix:** Add `COLLATE NOCASE` to the `name` column / query (migration
`002_channels.sql`), and make the in-memory store compare case-insensitively, matching
`strcasecmp`. Mirror the original "already in channel" guard before re-joining.

---

## F7 — Channel name validation diverges (length 64 vs MAX_CHANNELNAME_LEN; charset)
**Severity:** LOW
**Classification:** UNSURE / minor BUG

**Original ref:** `MAX_CHANNELNAME_LEN` bounds the wire read
(`handle_bnet.cpp:3652`). Original does almost no charset normalization — it accepts the
name largely as-is (only `\0`-terminated, length-bounded) and relies on `strcasecmp`
for matching.

**v3 ref:**
- `src/domain/chat/include/domain/chat/channel_name.hpp:38-45` — `ChannelName::parse`
  enforces 1..64 chars, printable ASCII 0x20–0x7E only.
- `src/application/chat/src/join_channel.cpp:18-21` — rejects only empty / `\0` / `\x01`;
  **does not actually use `ChannelName`** — so the strict 0x20–0x7E rule is *not* applied
  on the join path. The two validators disagree.

**Divergence:** `ChannelName` (strict) is bypassed by `JoinChannel` (lax). Net effect on
join is close to original (lax), but the codebase has an unused stricter validator that
could silently diverge if later wired. Also `MAX_CHANNELNAME_LEN` (original wire cap)
isn't cross-checked here, so v3 may accept names the legacy client/codec would reject (or
vice-versa). Confirm `MAX_CHANNELNAME_LEN` value to size the limit.

**Proposed fix:** Decide one rule. If keeping legacy leniency, drop/relax `ChannelName`;
if tightening, route the join name through `ChannelName::parse` and align the max length
with the codec's `MAX_CHANNELNAME_LEN`.

---

## F8 — Channel membership is not persisted; member list reconstructed empty on rehydrate
**Severity:** MEDIUM
**Classification:** BUG (correctness of EID_SHOWUSER / member broadcasts)

**Original ref:** membership lives in-memory on the channel
(`channel->memberlist`, `channel.cpp:452-456`); the live member set is always authoritative.

**v3 ref:**
- `src/infra/persistence/channel_repository.cpp:26-34,78-98` — `channel_from_row`
  rehydrates with `rehydrate(..., /*members*/{}, /*banlist*/{})` and `save` writes only
  `id,name,topic,flags,max_members` (no members table). 
- `src/application/chat/src/join_channel.cpp:60-76` — after `admit`, the channel is
  `save`d (members dropped) then **re-`find_by_name`'d**, returning a channel whose
  `members_` is empty.
- `fsm_chat.cpp:134-165` / `connection_fsm_inchannel.cpp:96-107` iterate
  `joined_channel.member_ids()` to emit EID_SHOWUSER for existing occupants.

**Divergence:** Because the reloaded channel has no members, the joining user is told the
channel is empty (no EID_SHOWUSER for anyone already there), and `members_to_notify`
(computed from the in-memory copy before save, `join_channel.cpp:79-91`) vs the reloaded
copy are inconsistent. In a SQL-backed deployment the user list is effectively broken;
with an in-memory store that preserves the object it may happen to work, masking the bug.

**Proposed fix:** Persist membership (join/leave rows) or, if channels are meant to be
purely in-memory live state, do not round-trip through `save`+`find_by_name` to obtain
the post-join snapshot — return the in-memory `channel` directly (it already has the
correct member set and assigned events).

---

## What MATCHES the original

- **ENTERCHAT server behaviour:** v3 echoes the reply and does not itself force a join,
  same as original (`fsm_chat.cpp:58-68`, `connection_fsm_loggedin.cpp:25-59`). See F3.
- **Public channel auto-create on join-miss exists** (`join_channel.cpp:26-40`) — the
  original also auto-creates a temp channel when the name isn't found
  (`connection.cpp:1998-2010`). The *gating* differs (F1 ask_new_channel), but the
  create-on-miss mechanism is present.
- **Full / banned / locked / wrong-tag rejection outcomes** are modelled in the domain
  (`channel.hpp:110-137`) and mapped to client errors (`fsm_chat.cpp:92-125`), mirroring
  the original's full/banned/admin-only checks (`connection.cpp:1958-1981`) — though the
  per-tag and full→rollover paths are neutered by F2/F4.
- **EID values** for SHOWUSER/JOIN/LEAVE/TALK/CHANNEL/INFO are centralized and correct
  (`fsm_chat.cpp:49-56`), an improvement over hand-typed legacy literals.
- **Idempotent re-join** (joining a channel you're already in is a no-op) is preserved in
  the domain (`channel.hpp:131-134`), matching `handle_bnet.cpp:3657-3658`.

---

## Priority summary
1. **F1** GENERIC/NORMAL join-flag handling (clan redirect + ask_new_channel) — HIGH
2. **F2** overflow rollover to numbered siblings — HIGH
3. **F4 + F5** permanent channels from channel.conf (metadata + actually loading it) — HIGH
4. **F6** case-insensitive name lookup — MEDIUM
5. **F8** membership persistence / post-join snapshot — MEDIUM
6. **F3 / F7** documented-but-fine / minor validation divergence — LOW
