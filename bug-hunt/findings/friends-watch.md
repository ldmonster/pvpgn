# Bug Hunt: Friends List / Mutual Friends / Watch-Notify

ORIGINAL: `/home/cnupt/work/pvpgn-server`
CURRENT v3: `/home/cnupt/work/pvpgn`
Subsystem: friends list + mutual friends + watch/notify
Date: 2026-06-25

## Executive summary

The v3 friends subsystem exists at the domain (`FriendList`) and application
(`AddFriend`/`RemoveFriend`/`ListFriends`) layers, and the wire codec for the
friends packets (`codec_friends.cpp`) is byte-faithful to the original. **But
the whole subsystem is dead code: it is never wired into the protocol FSM.**
`BnetFsm::on(FriendsListRequest)` returns `ok()` with no reply, the use-case
context has no friends fields, and `/friends` is only an alias in a routing
table with no handler. On top of that, two core semantic features — **mutual
friends** and the entire **watch/notify** subsystem — have no v3 equivalent at
all. Net effect: a connected client's friends list never populates, friend
add/remove never works over the wire, and friends never receive login / logout
/ enter-game / leave-game notifications.

---

## Finding 1 — FRIENDSLIST request produces no reply (subsystem unwired)

- Severity: CRITICAL
- Classification: BUG

Original `_client_friendslistreq` builds a full `SERVER_FRIENDSLISTREPLY`:
iterates `account_get_friend(account, i)` in roster order, appends name + status
struct (status byte, location byte, clienttag u32) + location-name string, and
sets `friendcount`.

ORIGINAL ref — `src/bnetd/handle_bnet.cpp:2298-2388`:
```cpp
packet_set_type(rpacket, SERVER_FRIENDSLISTREPLY);
for (i = 0; i < n; i++) {
    frienduid = account_get_friend(account, i);
    ...
    packet_append_string(rpacket, account_get_name(friend_get_account(fr)));
    ... build status (mutual/dnd/away), location (game/chan/online/offline) ...
    packet_append_data(rpacket, &status, sizeof(status));
    ... append game/channel/"" ...
}
bn_byte_set(&rpacket->u.server_friendslistreply.friendcount, friendcount);
conn_push_outqueue(c, rpacket);
```

v3 handler is a no-op stub — it validates state then returns `ok()` and sends
nothing:

V3 ref — `src/protocol/bnet/src/fsm/fsm_chat.cpp:340-345`:
```cpp
core::Status<> BnetFsm::on(const FriendsListRequest&) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: FRIENDSLIST before login");
    }
    return core::ok();          // <-- never builds/sends FriendsListReply
}
```

`on(FriendInfoRequest)` (`fsm_chat.cpp:347-352`) is the same no-op stub.

Divergence: client requests its friends list and receives nothing; the in-client
friends UI stays empty forever. The `ListFriends` use-case
(`src/application/social/src/list_friends.cpp`) that *would* feed this reply is
never called — `BnetUseCaseContext`
(`src/protocol/bnet/include/protocol/bnet/use_case_context.hpp:43-60`) has no
`list_friends` / `add_friend` / `remove_friend` field, unlike `list_channels`
which IS wired (`fsm_chat.cpp:375-397`).

Proposed fix: add `ListFriends`/`AddFriend`/`RemoveFriend` to
`BnetUseCaseContext`, populate them in the FSM factory, and have
`on(FriendsListRequest)` call `list_friends->execute(owner)`, translate each
`FriendInfo` into a `FriendsListEntry`, and `ctx_->send(FriendsListReply{...})`.

---

## Finding 2 — Mutual-friend detection completely absent in v3

- Severity: HIGH
- Classification: BUG (feature regression)

Original tracks a tri-state `mutual` flag per friend
(`FRIEND_UNLOADEDMUTUAL=-1`, `FRIEND_NOTMUTUAL=0`, `FRIEND_ISMUTUAL=1`,
`src/bnetd/friends.h:20-22`). It is computed at add-time
(`account_add_friend`, `src/bnetd/account_wrap.cpp:1932-1935`):
```cpp
if (account_check_mutual(facc, my_uid) == 0)
    friendlist_add_account(flist, facc, FRIEND_ISMUTUAL);
else
    friendlist_add_account(flist, facc, FRIEND_NOTMUTUAL);
```
and surfaced as the `FRIEND_TYPE_MUTUAL` (0x01) bit in every status byte of
FRIENDSLIST / FRIENDINFO / FRIENDADD replies, e.g.
`handle_bnet.cpp:2348-2349`:
```cpp
if ((friend_get_mutual(fr)))
    stat |= FRIEND_TYPE_MUTUAL;
```

v3 `FriendList` stores a flat `std::vector<AccountId>` with **no mutual concept
at all**:

V3 ref — `src/domain/social/include/domain/social/friend_list.hpp:61-64`:
```cpp
AccountId                        owner_;
std::vector<AccountId>           friends_;
std::vector<events::DomainEvent> events_;
```
`FriendInfo` (`src/application/social/include/application/social/list_friends.hpp:20-26`)
likewise carries only `id/name/is_online/current_channel/current_game` — no
mutual, no dnd, no away. The wire constant `kFriendTypeMutual` exists in
`anongame_wire_types.hpp:126` but nothing ever sets it.

Divergence: even once Finding 1 is wired, the status byte will always report
NON_MUTUAL (and never DND/AWAY). Mutual-only behaviour — the "Your friend X has
entered…" whisper in the original only fires for mutual friends, and the
arranged-team screen only lists mutual friends — cannot be reproduced.

Proposed fix: restore a per-entry mutual flag in `FriendList`
(or compute it in `ListFriends` by checking whether the target's friend list
contains the owner), and map it to `kFriendTypeMutual` when building entries.
Also surface DND/AWAY from session state into the status byte.

---

## Finding 3 — Watch/notify subsystem (friend & watch notifications) absent

- Severity: HIGH
- Classification: BUG (feature regression) / UNSURE if intentionally deferred

Original `watch.cpp` implements `WatchComponent::dispatch_whisper`, which on
login/logout/join-game/leave-game events whispers the acting user's MUTUAL
friends ("Your friend %s has entered %s.", etc.) and also notifies explicit
watchers ("Watched user %s …").

ORIGINAL ref — `src/bnetd/watch.cpp:147-217` (mutual-friend loop at 169-186,
gated by `if (friend_get_mutual(fr))` at line 183; watcher loop at 211-217).

v3: a repo-wide search for `mutual`, "your friend", "has entered", and any
`Watch`/`dispatch_whisper` analog in the social/domain layers returns nothing.
`/watch` and `/unwatch` appear only as alias rows in
`src/application/admin_commands/src/router.cpp:60-61` with no implementing
handler. No friend-login/logout notification path exists.

Divergence: friends never get the "your friend logged in / entered a game"
whispers; `/watch` is a dead alias. This is a user-visible behavioural loss.

Proposed fix: port a watch component that subscribes to session
login/logout/game events and dispatches whispers to mutual friends + explicit
watchers; wire `/watch` / `/unwatch` to it.

---

## Finding 4 — Max-friends limit hardcoded to 25, not configurable (and wrong default)

- Severity: MEDIUM
- Classification: BUG

Original limit is the **configurable** pref `max_friends`, default **20**
(`conf/bnetd.conf.in:409` `max_friends = 20`; fallback constant
`src/common/setup_before.h:170` `const unsigned MAX_FRIENDS = 20;`;
enforced in `account_add_friend`, `account_wrap.cpp:1919-1921`:
```cpp
int nf = account_get_friendcount(my_acc);
if (nf >= prefs_get_max_friends())
    return -3;
```

v3 hardcodes the cap and uses a different value:

V3 ref — `src/domain/social/include/domain/social/friend_list.hpp:21,43`:
```cpp
static constexpr std::size_t kMaxFriends = 25;     // legacy cap claim
...
if (friends_.size() >= kMaxFriends) return AddOutcome::Full;
```

Divergence: (a) not configurable — admins lose `max_friends`; (b) default 25 vs
original 20, so users can add 21–25 friends an original server would reject. The
comment "Legacy Battle.net cap" is misleading (PvPGN default is 20). Note: the
codec accepts up to 200 entries on the wire
(`codec_friends.cpp:21`), so the cap is purely a domain rule.

Proposed fix: make the cap injectable/configurable (plumb `max_friends` from
config into `AddFriend`/`FriendList`) and set the default to 20 to match the
original.

---

## Finding 5 — No reciprocal "added you" notification on friend add

- Severity: MEDIUM
- Classification: BUG (feature regression)

Original `_handle_friends_command` add path notifies the target if online:
"X added you to his/her friends list."

ORIGINAL ref — `src/bnetd/command.cpp:1519-1523`:
```cpp
dest_c = connlist_find_connection_by_account(friend_acc);
if (dest_c != NULL) {
    msgtemp = localize(c, "{} added you to his/her friends list.", conn_get_username(c));
    message_send_text(dest_c, message_type_info, dest_c, msgtemp);
}
```

v3 `AddFriend::execute` (`src/application/social/src/add_friend.cpp:10-58`) only
mutates the owner's list, saves, and publishes a `FriendAdded` domain event. No
subscriber sends the reciprocal notification (and as noted in Finding 1 the use
case is not even invoked from the protocol layer).

Divergence: the added user is never told. Combined with Finding 2, mutual-status
transitions are also not pushed.

Proposed fix: on `FriendAdded`, look up the target's live session and whisper
the "added you" message; also re-evaluate mutual flags for both parties.

---

## Finding 6 — No reciprocal mutual-flag cleanup on friend remove

- Severity: MEDIUM
- Classification: BUG (feature regression)

Original `account_remove_friend2` demotes the *other* user's view of me from
mutual back to non-mutual when I remove them.

ORIGINAL ref — `src/bnetd/account_wrap.cpp:1991-1999`:
```cpp
t_account * facc = friend_get_account(fr);
t_list * fflist = account_get_friends(facc);
t_friend * ffr = friendlist_find_account(fflist, account);
account_remove_friend(account, i);
if (facc && fflist && ffr)
    friend_set_mutual(ffr, FRIEND_NOTMUTUAL);   // demote the other side
friendlist_remove_friend(flist, fr);
```

v3 `RemoveFriend::execute` (`src/application/social/src/remove_friend.cpp:9-37`)
just erases the entry from the owner's own list and publishes `FriendRemoved`.
No counterpart update (consistent with Finding 2: there is no mutual state to
update).

Divergence: once mutual support is restored, removing a friend would leave the
other party still flagged as mutual — a stale-flag bug. Flagging now so it is
handled when mutual support is reintroduced.

Proposed fix: when removing, re-evaluate / demote the counterpart's mutual flag.

---

## Finding 7 — Friend roster ordering & move (/friends move) not modeled

- Severity: LOW
- Classification: UNSURE (likely intentional simplification, but breaks parity)

Original maintains an explicit ordered roster via `account_get_friend(acc, i)` /
`account_set_friend` and supports reordering (FRIENDMOVE, the up/down handlers in
`command.cpp:1666-1725`). FRIENDSLIST is emitted in that stored order. v3
`FriendList` preserves vector insertion order (so order is *stable*), but there
is no move/reorder operation and no `FriendMoveAck` producer wired (the codec
`encode(FriendMoveAck)` at `codec_friends.cpp:289-294` exists but is unused).

Divergence: `/f move` / drag-reorder in client has no effect. Low impact.

Proposed fix: add a reorder op on `FriendList` and wire `FriendMoveAck` if
client-side reorder parity is desired.

---

## What MATCHES (verified correct)

- **Wire encoding of FRIENDSLIST reply entries** — v3 codec emits exactly
  name(cstr) + status(u8) + location(u8) + clienttag(u32) + location_name(cstr),
  count-prefixed, identical field order/size to the original struct
  (`codec_friends.cpp:244-255` vs `handle_bnet.cpp:2336-2381`). The *encoding*
  is faithful; only the *population* (Findings 1-2) is missing.
- **FRIENDINFO reply layout** — friend_num/type/status/clienttag/game_name order
  matches (`codec_friends.cpp:263-271` vs `handle_bnet.cpp:2430-2469`).
- **FRIENDADD ack layout** — name/status/location/clienttag/location_name matches
  (`codec_friends.cpp:273-281` vs `command.cpp:1528-1573`).
- **FRIENDDEL ack** — single friend_num byte (`codec_friends.cpp:283-287` vs
  `command.cpp:1639`).
- **Friend type / status constants** — all values identical:
  NonMutual 0x00, Mutual 0x01, Dnd 0x02, Away 0x04; status Offline 0x00,
  Online 0x01, Chat 0x02, PublicGame 0x03, PrivateGame 0x05
  (`anongame_wire_types.hpp:125-134` vs
  `common/anongame_protocol.h:642-683`). Note the original's intentional
  PrivateGame = 0x05 (skipping 0x04) is preserved.
- **Domain add rules** — self-add → `Self`, duplicate → `AlreadyPresent`,
  over-cap → `Full`, otherwise `Added`
  (`friend_list.hpp:40-47`) mirror the original's -2/-4/-3/0 returns
  (`account_wrap.cpp:1916-1937`). Only the cap *value/configurability* diverges
  (Finding 4).
- **Add-nonexistent rule** — v3 `AddFriend` rejects with `TargetNotFound`
  (`add_friend.cpp:18-21`), matching original's "That user does not exist."
  (`command.cpp:1495-1498`).
- **Remove-nonexistent rule** — v3 `NotAFriend` (`remove_friend.cpp:20-22`)
  matches original's "X was not found on your friends list." (-2,
  `command.cpp:1628-1631`).

## Wave 56: friend presence watch implemented
Verified differentially (tests/diff/diff_friends_watch.py): on a mutual friend's
login/logout, the user receives an EID_WHISPER "Your friend X has entered/left
<server>." — matching the original's watch.cpp dispatch_whisper (gated on mutual
friendship + online). Implemented as BnetFsm::notify_friends_presence(entered),
called from the OLS+W3 login success paths and on_disconnect; mutual+online
resolution via the list_friends use-case; server name from cfg.server_name.
The separate explicit /watch watchlist path ("Watched user %s ...") is still not
implemented (distinct from the friends-list presence push covered here).
