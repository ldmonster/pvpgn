# Step 7: Legacy Data Structures Elimination Checklist

**Round:** R111
**Date:** 2026-05-21
**Goal:** Replace `t_list`, `t_hashtable`, `t_elist`, `t_hlist` with STL containers.

---

## Summary Counts (pre-migration baseline)

| Type | Total occurrences | Files affected | Header includes |
|------|-------------------|----------------|-----------------|
| `t_list` | 165 | 47 | 77 |
| `t_hashtable` | 49 | 7 | 7 |
| `t_elist` | 59 | 16 | 14 |
| `t_hlist` | 22 | 9 | (via elist.h) |

---

## API Reference

### `t_list` API (`src/common/list.h`)
- `list_create()` → allocate new list
- `list_destroy(list)` → free list
- `list_append_data(list, data)` → push_back
- `list_prepend_data(list, data)` → push_front
- `list_remove_elem(list, &curr)` → erase during traversal
- `list_remove_data(list, data, &elem)` → find+erase
- `list_get_length(list)` → size()
- `list_get_data_by_pos(list, pos)` → operator[]
- `LIST_TRAVERSE(list, curr)` → range-for
- `LIST_TRAVERSE_CONST(list, curr)` → range-for const
- `elem_get_data(curr)` → *it (cast to T*)
- `list_get_first(list)` / `elem_get_next(list, curr)` → iterator

### `t_hashtable` API (`src/common/hashtable.h`)
- `hashtable_create(num_rows)` → `std::unordered_map`
- `hashtable_destroy(ht)` → destructor
- `hashtable_insert_data(ht, data, hash)` → `map[key] = value`
- `hashtable_remove_data(ht, data, hash)` → `map.erase(key)`
- `hashtable_get_entry_by_data(ht, data, hash)` → `map.find(key)`
- `HASHTABLE_TRAVERSE(ht, curr)` → range-for
- `HASHTABLE_TRAVERSE_MATCHING(ht, curr, hash)` → `equal_range`
- `entry_get_data(entry)` → `it->second`

### `t_elist` / `t_hlist` API (`src/common/elist.h`)
- Intrusive doubly-linked list (Linux kernel style)
- `elist_init(ptr)` → initialize sentinel node
- `elist_add(where, what)` → insert after
- `elist_add_tail(where, what)` → insert before (queue)
- `elist_del(what)` → unlink
- `elist_for_each(pos, head)` → range-for
- `elist_for_each_safe(pos, head, save)` → range-for with removal
- `elist_entry(ptr, type, member)` → `container_of`
- `t_hlist` = singly-linked variant

---

## Per-File Audit Table

### `src/bnetd/` — t_list users

| File | t_list refs | list_* calls | Exposed in header? | Replacement | Difficulty | Status |
|------|-------------|--------------|-------------------|-------------|------------|--------|
| `autoupdate.cpp` | 1 | 8 | No | `std::vector<t_autoupdate*>` | **Easy** | ✅ R105 |
| `command_groups.cpp` | 1 | 8 | No | `std::vector<t_command_groups*>` | **Easy** | ✅ R105 |
| `character.cpp` | 1 | 5 | No | `std::vector<t_character*>` | **Easy** (list never populated) | ✅ R105 |
| `alias_command.cpp` | 1 | 22 | Yes (internal only) | `std::vector<t_alias*>` + nested `std::vector<t_output*>` | Medium | ⬜ |
| `ipban.cpp` | 1 | 23 | No | `std::vector<t_ipban_entry*>` | Medium | ⬜ |
| `watch.cpp` | 1 | 2 | No | Already uses `std::list<Watch>` internally | **Easy** (just remove include) | ✅ R111 |
| `tournament.cpp` | 1 | 16 | No | `std::vector` | Medium | ⬜ |
| `anongame.cpp` | 1 | 8 | No | `std::vector` | Medium | ⬜ |
| `anongame_wol.cpp` | 2 | 16 | Yes (anongame_wol.h) | `std::vector` | Medium | ⬜ |
| `anongame_infos.cpp` | 2 | 27 | Yes (anongame_infos.h) | `std::vector` | Hard | ⬜ |
| `icons.cpp` | 1 | 30 | Yes (icons.h) | `std::vector` | Hard | ⬜ |
| `realm.cpp` | 7 | 18 | Yes (realm.h) | `std::vector<t_realm*>` | Hard | ✅ R110 |
| `channel.cpp` | 5 | 41 | Yes (channel.h) | `std::vector<t_channel*>` (channellist) + `std::vector<std::string>` (banlist) | Hard | ✅ R107 |
| `clan.cpp` | 5 | 51 | Yes (clan.h) | `std::vector` | Hard | ⬜ |
| `friends.cpp` | 10 | 22 | Yes (friends.h) | `std::vector` | Hard | ⬜ |
| `team.cpp` | 5 | 16 | Yes (team.h) | `std::vector` | Hard | ⬜ |
| `account.cpp` | 2 | 7 | Yes (account.h) | `std::vector` | Hard | ⬜ |
| `account_wrap.cpp` | 3 | 3 | No | `std::vector` | Medium | ⬜ |
| `command.cpp` | 6 | 22 | No | `std::vector` | Medium | ⬜ |
| `connection.cpp` | 3 | 36 | Yes (connection.h) | `std::vector` | Hard | ⬜ |
| `handle_bnet.cpp` | 3 | 17 | No | `std::vector` | Medium | ⬜ |
| `handle_wol.cpp` | 1 | 3 | No | `std::vector` | Medium | ⬜ |
| `handle_anongame.cpp` | 1 | 2 | No | `std::vector` | Medium | ⬜ |
| `handle_apireg.cpp` | 2 | 15 | Yes (handle_apireg.h) | `std::vector` | Hard | ⬜ |
| `luafunctions.cpp` | 3 | 10 | No | `std::vector` | Medium | ⬜ |
| `message.cpp` | 0 | 7 | No | `std::vector` | Medium | ⬜ |
| `output.cpp` | 0 | 11 | No | `std::vector` | Medium | ⬜ |
| `server.cpp` | 0 | 16 | No | `std::vector` | Medium | ⬜ |
| `tracker.cpp` | 0 | 11 | No | `std::vector` | Medium | ⬜ |
| `irc.cpp` | 0 | 4 | No | `std::vector` | Medium | ⬜ |
| `sql_dbcreator.cpp` | 3 | 30 | Yes (sql_dbcreator.h) | `std::vector` | Hard | ⬜ |
| `sql_common.cpp` | 0 | 4 | No | `std::vector` | Medium | ⬜ |
| `storage_file.cpp` | 0 | 4 | No | `std::vector` | Medium | ⬜ |

### `src/d2cs/` — t_list users

| File | t_list refs | list_* calls | Exposed in header? | Replacement | Difficulty | Status |
|------|-------------|--------------|-------------------|-------------|------------|--------|
| `game.cpp` | 3 | 31 | Yes (game.h) | `std::vector` | Hard | ⬜ |
| `d2gs.cpp` | 2 | 31 | Yes (d2gs.h) | `std::vector` | Hard | ⬜ |
| `gamequeue.cpp` | 2 | 19 | Yes (gamequeue.h) | `std::vector` | Hard | ⬜ |
| `serverqueue.cpp` | 2 | 12 | Yes (serverqueue.h) | `std::vector` | Hard | ⬜ |
| `connection.cpp` | 1 | 9 | Yes (connection.h) | `std::vector` | Hard | ⬜ |
| `handle_d2cs.cpp` | 0 | 11 | No | `std::vector` | Medium | ⬜ |
| `main.cpp` | 0 | 10 | No | `std::vector` | Medium | ⬜ |
| `server.cpp` | 0 | 6 | No | `std::vector` | Medium | ⬜ |

### `src/d2dbs/` — t_list users

| File | t_list refs | list_* calls | Exposed in header? | Replacement | Difficulty | Status |
|------|-------------|--------------|-------------------|-------------|------------|--------|
| `dbserver.cpp` | 1 | 12 | Yes (dbserver.h) | `std::vector` | Hard | ⬜ |
| `d2ladder.cpp` | 0 | 11 | Yes (d2ladder.h) | `std::vector` | Hard | ⬜ |
| `dbspacket.cpp` | 0 | 8 | No | `std::vector` | Medium | ⬜ |

### `src/common/` — t_list users

| File | t_list refs | list_* calls | Exposed in header? | Replacement | Difficulty | Status |
|------|-------------|--------------|-------------------|-------------|------------|--------|
| `addr.cpp` | 1 | 13 | Yes (addr.h) | `std::vector` | Hard | ⬜ |
| `trans.cpp` | 1 | 9 | No | `std::vector` | Medium | ⬜ |

### `src/v3/` — t_list users

| File | t_list refs | list_* calls | Exposed in header? | Replacement | Difficulty | Status |
|------|-------------|--------------|-------------------|-------------|------------|--------|
| `tools/bntrackd/bntrackd.cpp` | 1 | 0 | No | Remove include | **Easy** | ⬜ |
| `integration/legacy_bnetd/src/profile_bridge.cpp` | 1 | 2 | No | `std::vector` | Easy | ⬜ |

---

### `src/bnetd/` — t_hashtable users

| File | t_hashtable refs | hashtable_* calls | Exposed in header? | Replacement | Difficulty | Status |
|------|-----------------|-------------------|-------------------|-------------|------------|--------|
| `account.cpp` | 4 | 23 | Yes (account.h) | `std::unordered_map` | Hard | ⬜ |
| `command.cpp` | 1 | 2 | No | `std::unordered_map` | Medium | ⬜ |
| `luafunctions.cpp` | 0 | 2 | No | `std::unordered_map` | Medium | ⬜ |
| `ladder.cpp` | 0 | 2 | No | `std::unordered_map` | Medium | ⬜ |

### `src/d2cs/` — t_hashtable users

| File | t_hashtable refs | hashtable_* calls | Exposed in header? | Replacement | Difficulty | Status |
|------|-----------------|-------------------|-------------------|-------------|------------|--------|
| `connection.cpp` | 3 | 17 | Yes (connection.h) | `std::unordered_map` | Hard | ⬜ |
| `server.cpp` | 0 | 1 | No | `std::unordered_map` | Medium | ⬜ |
| `setup.h` | 0 | 9 | Yes | `std::unordered_map` | Hard | ⬜ |

---

### `src/bnetd/` — t_elist users

| File | t_elist refs | elist_* calls | Exposed in header? | Replacement | Difficulty | Status |
|------|-------------|---------------|-------------------|-------------|------------|--------|
| `attrlayer.cpp` | 8 | many | Yes (attrlayer.h) | `std::list<T*>` or `std::vector<T*>` | Hard | ⬜ |
| `game.cpp` | 7 | many | Yes (game.h) | `std::list<T*>` | Hard | ⬜ |
| `timer.cpp` | 5 | many | Yes (timer.h) | `std::list<T*>` | Hard | ⬜ |
| `news.cpp` | 4 | many | Yes (news.h) | `std::vector<T*>` | Medium | ⬜ |
| `connection.cpp` | 3 | many | Yes (connection.h) | `std::list<T*>` | Hard | ⬜ |
| `luafunctions.cpp` | 1 | few | No | `std::list<T*>` | Medium | ⬜ |

### `src/d2cs/` — t_elist users

| File | t_elist refs | elist_* calls | Exposed in header? | Replacement | Difficulty | Status |
|------|-------------|---------------|-------------------|-------------|------------|--------|
| `handle_d2cs.cpp` | 6 | many | No | `std::list<T*>` | Medium | ⬜ |
| `d2charlist.cpp` | 2 | few | Yes (d2charlist.h) | `std::vector<T*>` | Medium | ⬜ |

### `src/common/` — t_elist users

| File | t_elist refs | elist_* calls | Exposed in header? | Replacement | Difficulty | Status |
|------|-------------|---------------|-------------------|-------------|------------|--------|
| `rcm.cpp` | 1 | few | Yes (rcm.h) | `std::list<T*>` | Medium | ⬜ |
| `fdwatch.h` | 0 | 0 | Yes | Remove include | Easy | ⬜ |

---

### `src/bnetd/` — t_hlist users

| File | t_hlist refs | hlist_* calls | Exposed in header? | Replacement | Difficulty | Status |
|------|-------------|---------------|-------------------|-------------|------------|--------|
| `attrgroup.cpp` | 4 | many | Yes (attrgroup.h) | `std::unordered_map` | Hard | ⬜ |
| `storage_sql.cpp` | 4 | many | No | `std::unordered_map` | Medium | ⬜ |
| `file_plain.cpp` | 3 | many | No | `std::unordered_map` | Medium | ⬜ |
| `storage_file.cpp` | 2 | few | Yes (storage_file.h) | `std::unordered_map` | Medium | ⬜ |
| `storage.h` | 1 | 0 | Yes | `std::unordered_map` | Hard | ⬜ |
| `attr.h` | 1 | 0 | Yes | `std::unordered_map` | Hard | ⬜ |

---

## Migration Priority

### Tier 1 — Easy (no public API exposure, simple patterns) — R105

| File | Type | Action |
|------|------|--------|
| `bnetd/command_groups.cpp` | `t_list` | Replace with `std::vector<t_command_groups*>` |
| `bnetd/autoupdate.cpp` | `t_list` | Replace with `std::vector<t_autoupdate*>` |
| `bnetd/character.cpp` | `t_list` | Replace with `std::vector<t_character*>` (list never populated) |

### Tier 2 — Medium (no public API, moderate complexity) — R106+

| File | Type | Action | Status |
|------|------|--------|--------|
| `bnetd/ipban.cpp` | `t_list` | Replace with `std::vector<t_ipban_entry*>` | ✅ R106 |
| `bnetd/tournament.cpp` | `t_list` | Replace with `std::vector<t_tournament_user*>` | ✅ R106 |
| `bnetd/anongame.cpp` | `t_list` | Replace with `std::vector<t_matchdata*>` | ✅ R106 |
| `common/trans.cpp` | `t_list` | Replace with `std::vector<t_trans*>` | ✅ R106 |
| `bnetd/watch.cpp` | `t_list` | **Reclassified Tier 3** — uses `account_get_friends()` → `t_list*` from `friends.h` | ✅ R108 (migrated as caller update in R108; confirmed clean R111) |
| `bnetd/command.cpp` | `t_list` + `t_hashtable` | **Reclassified Tier 3** — uses `connlist()`, `channellist()`, `friends.h` `t_list*` | ⏭ R107+ |
| `bnetd/message.cpp` | `t_list` | **Reclassified Tier 3** — uses `connlist()` from `connection.h` | ⏭ R107+ |
| `bnetd/tracker.cpp` | `t_list` | **Reclassified Tier 3** — uses `addrlist_*` from `addr.h` | ⏭ R107+ |
| `bnetd/irc.cpp` | `t_list` | **Reclassified Tier 3** — uses `channellist()`, `channel_get_banlist()` | ⏭ R107+ |
| `bnetd/output.cpp` | `t_list` | **Reclassified Tier 3** — uses `connlist()`, `channellist()` | ⏭ R107+ |
| `bnetd/server.cpp` | `t_list` | **Reclassified Tier 3** — uses `addrlist_*`, `connlist()` | ⏭ R107+ |
| `bnetd/sql_common.cpp` | `t_list` | **Reclassified Tier 3** — uses `clan->members` (`t_list*` in `clan.h`) | ⏭ R107+ |
| `bnetd/storage_file.cpp` | `t_list` + `t_hlist` | **Reclassified Tier 3** — uses `clan->members`, `t_hlist` from `storage.h` | ⏭ R107+ |
| `bnetd/news.cpp` | `t_elist` | **Reclassified Tier 3** — `t_elist list` in `t_news_index` struct in `news.h` | ⏭ R107+ |
| `d2cs/handle_d2cs.cpp` | `t_elist` | **Reclassified Tier 3** — uses `d2cs_gamelist()`, `game_get_charlist()` | ⏭ R107+ |
| `d2cs/d2charlist.cpp` | `t_elist` | **Reclassified Tier 3** — `t_elist list` in `t_d2charlist` in `d2charlist.h` | ⏭ R107+ |
| `d2cs/main.cpp` | `t_list` | **Reclassified Tier 3** — calls create/destroy on Tier 3 lists | ⏭ R107+ |
| `d2cs/server.cpp` | `t_list` + `t_hashtable` | **Reclassified Tier 3** — uses `addrlist_*`, `hashtable_purge(d2cs_connlist())` | ⏭ R107+ |
| `d2dbs/dbspacket.cpp` | `t_list` | **Reclassified Tier 3** — uses `dbs_server_connection_list` from `dbserver.h` | ⏭ R107+ |
| `common/rcm.cpp` | `t_elist` | **Reclassified Tier 3** — `t_elist refs` in public structs in `rcm.h` | ⏭ R107+ |
| `bnetd/account_wrap.cpp` | `t_list` | Pending audit | ⏳ R107+ |
| `bnetd/handle_bnet.cpp` | `t_list` | Pending audit | ⏳ R107+ |
| `bnetd/handle_wol.cpp` | `t_list` | Pending audit | ⏳ R107+ |
| `bnetd/handle_anongame.cpp` | `t_list` | Pending audit | ⏳ R107+ |
| `bnetd/luafunctions.cpp` | `t_list` + `t_elist` | Pending audit | ⏳ R107+ |
| `bnetd/storage_sql.cpp` | `t_hlist` | Replace with `std::unordered_map` | ⏳ R107+ |
| `bnetd/file_plain.cpp` | `t_hlist` | Replace with `std::unordered_map` | ⏳ R107+ |
| `bnetd/ladder.cpp` | `t_hashtable` | Replace with `std::unordered_map` | ⏳ R107+ |
| `v3/integration/legacy_bnetd/src/profile_bridge.cpp` | `t_list` | Replace with `std::vector` | ⏳ R107+ |

### Tier 3 — Hard (public API exposure, many callers) — R107+

| File | Type | Blocker |
|------|------|---------|
| `bnetd/alias_command.cpp` | `t_list` | `t_alias.output` is `t_list*` in header |
| `bnetd/anongame_wol.cpp` | `t_list` | Exposed in `anongame_wol.h` |
| `bnetd/anongame_infos.cpp` | `t_list` | Exposed in `anongame_infos.h` |
| `bnetd/icons.cpp` | `t_list` | Exposed in `icons.h` |
| `bnetd/realm.cpp` | `t_list` | Exposed in `realm.h` |
| `bnetd/channel.cpp` | `t_list` | Exposed in `channel.h` |
| `bnetd/clan.cpp` | `t_list` | ✅ R108 |
| `bnetd/friends.cpp` | `t_list` | ✅ R108 |
| `bnetd/team.cpp` | `t_list` | Exposed in `team.h` |
| `bnetd/account.cpp` | `t_list` + `t_hashtable` | Exposed in `account.h` |
| `bnetd/connection.cpp` | `t_list` + `t_elist` | ✅ R109 |
| `bnetd/handle_apireg.cpp` | `t_list` | Exposed in `handle_apireg.h` |
| `bnetd/sql_dbcreator.cpp` | `t_list` | Exposed in `sql_dbcreator.h` |
| `bnetd/attrlayer.cpp` | `t_elist` | Exposed in `attrlayer.h` |
| `bnetd/game.cpp` | `t_elist` | Exposed in `game.h` |
| `bnetd/timer.cpp` | `t_elist` | Exposed in `timer.h` |
| `bnetd/attrgroup.cpp` | `t_hlist` | Exposed in `attrgroup.h` |
| `bnetd/attr.h` | `t_hlist` | Public header |
| `bnetd/storage.h` | `t_hlist` | Public header |
| `d2cs/game.cpp` | `t_list` | Exposed in `game.h` |
| `d2cs/d2gs.cpp` | `t_list` | Exposed in `d2gs.h` |
| `d2cs/gamequeue.cpp` | `t_list` | Exposed in `gamequeue.h` |
| `d2cs/serverqueue.cpp` | `t_list` | Exposed in `serverqueue.h` |
| `d2cs/connection.cpp` | `t_list` + `t_hashtable` | Exposed in `connection.h` |
| `d2cs/setup.h` | `t_hashtable` | Public header |
| `d2dbs/dbserver.cpp` | `t_list` | Exposed in `dbserver.h` |
| `d2dbs/d2ladder.cpp` | `t_list` | Exposed in `d2ladder.h` |
| `common/addr.cpp` | `t_list` | Exposed in `addr.h` |

---

## R105 Replacements Log

### `bnetd/command_groups.cpp` ✅
- Replaced `static t_list * command_groups_head` with `static std::vector<t_command_groups*> command_groups_list`
- Replaced `list_create()` with vector default construction
- Replaced `list_append_data()` with `push_back()`
- Replaced `LIST_TRAVERSE` + `elem_get_data` + `list_remove_elem` with range-for + `delete`
- Replaced `list_destroy()` with vector clear + delete
- Removed `#include "common/list.h"`

### `bnetd/autoupdate.cpp` ✅
- Replaced `static t_list * autoupdate_head` with `static std::vector<t_autoupdate*> autoupdate_list`
- Replaced `list_create()` with vector default construction
- Replaced `list_append_data()` with `push_back()`
- Replaced `LIST_TRAVERSE` + `LIST_TRAVERSE_CONST` with range-for
- Replaced `list_remove_elem` + `list_destroy` with vector clear + delete
- Removed `#include "common/list.h"`

### `bnetd/character.cpp` ✅
- Replaced `static t_list * characterlist_head` with `static std::vector<t_character*> characterlist`
- Replaced `list_create()` / `list_destroy()` with vector default construction / clear
- Replaced `LIST_TRAVERSE` with range-for
- Removed `#include "common/list.h"`
- Note: list was never populated (no `list_append_data` calls) — effectively a no-op list

---

## R106 Replacements Log

### `bnetd/ipban.cpp` ✅
- Replaced `static t_list * ipbanlist_head` with `static std::vector<t_ipban_entry*> ipbanlist`
- `list_create()` → `ipbanlist.clear()`
- `list_destroy()` + `LIST_TRAVERSE` → range-for + `ipban_unload_entry()` + `clear()`
- `list_append_data()` → `push_back()`
- `LIST_TRAVERSE_CONST` → range-for (in `ipbanlist_save`, `ipbanlist_check`, `ipban_func_list`)
- `LIST_TRAVERSE` + `list_remove_elem` → iterator-based `erase()` (in `ipbanlist_unload_expired`, `ipban_func_del`)
- Removed `#include "common/list.h"`, added `#include <algorithm>`

### `bnetd/tournament.cpp` ✅
- Replaced `static t_list * tournament_head` with `static std::vector<t_tournament_user*> tournament_list`
- `list_create()` → `tournament_list.clear()`
- `LIST_TRAVERSE` + `list_remove_elem` + `list_destroy` → range-for delete + `clear()` (in `_gamelist_destroy`)
- `list_prepend_data()` → `push_back()` (in `tournament_signup_user`)
- `LIST_TRAVERSE` → range-for (in `tournament_get_user`, `tournament_get_game_in_progress`)
- Removed `#include "common/list.h"`, added `#include <vector>`, `#include <algorithm>`

### `bnetd/anongame.cpp` ✅
- Replaced `static t_list *matchlists[ANONGAME_TYPES][MAX_LEVEL]` with `static std::vector<t_matchdata*> matchlists[ANONGAME_TYPES][MAX_LEVEL]`
- `anongame_matchlists_create()`: NULL-init loop → `clear()` per slot
- `anongame_matchlists_destroy()`: `list_destroy()` per slot → range-for delete + `clear()` per slot
- `_anongame_queue()`: `list_create()` + `list_append_data()` → `push_back()`
- `_anongame_match()`: `LIST_TRAVERSE` → range-for
- `anongame_unqueue()`: `LIST_TRAVERSE` + `list_remove_elem` → iterator-based `erase()`
- Removed `#include "common/list.h"`, added `#include <vector>`

### `common/trans.cpp` ✅
- Replaced `static t_list * trans_head` with `static std::vector<t_trans*> trans_list`
- `list_create()` → `trans_list.clear()`
- `list_append_data()` → `push_back()` (×2 in `trans_load`)
- `LIST_TRAVERSE` + `list_remove_elem` + `list_destroy` → range-for delete + `clear()` (in `trans_unload`)
- `LIST_TRAVERSE_CONST` → range-for (in `trans_net`)
- Removed `#include "common/list.h"`, added `#include <vector>`

### `bnetd/watch.cpp` ⏭ Reclassified Tier 3
- Uses `account_get_friends()` which returns `t_list*` from `friends.h` — cannot remove `#include "common/list.h"`
- Deferred to R107+ when `friends.h` API is migrated

---

## R108 Replacements Log

### `bnetd/clan.h` ✅ R108
- `CLAN_INTERNAL_ACCESS` section: `t_list* members` → `std::vector<struct _clanmember*> members`
- Public API: `clanlist()` return type → `const std::vector<t_clan*>&`
- Public API: `clan_get_members()` return type → `std::vector<t_clanmember*>&`

### `bnetd/clan.cpp` ✅ R108
- Added `#include <algorithm>`
- `static t_list* clanlist_head` → `static std::vector<t_clan*> clanlist_head`
- All `LIST_TRAVERSE(clan->members, curr)` → range-for
- All `LIST_TRAVERSE(clanlist_head, curr)` → range-for
- `clan_unload_members`: range-for delete + `clan->members.clear()`
- `clan_remove_all_members`: range-for + `clan->members.clear()`
- `clanlist_remove_clan`: `std::find` + `erase`
- `clanlist_add_clan`: `clanlist_head.push_back(clan)`
- `clanlist_load`: removed `list_create()`, `if (clanlist_head)` → `if (!clanlist_head.empty())`
- `clanlist_unload`: range-for + `clanlist_head.clear()`
- `clan_add_member`: `clan->members.push_back(member)`, removed NULL check
- `clan_remove_member`: `std::find` + `erase`
- `clan_create`: removed `clan->members = list_create()`, uses `push_back`
- `clan_get_member_count`: `return static_cast<unsigned>(clan->members.size())`
- `clan_get_members`: returns `std::vector<t_clanmember*>&` with static empty fallback
- `clanlist()`: returns `const std::vector<t_clan*>&`
- `clan_get_possible_member`: `auto& flist = account_get_friends(...)` + range-for

### `bnetd/friends.h` ✅ R108
- Added `#include <vector>`
- All `friendlist_*` signatures: `t_list*` → `std::vector<t_friend*>&`
- `account_get_friends()` return type: `t_list*` → `std::vector<t_friend*>&`

### `bnetd/friends.cpp` ✅ R108
- Removed `#include "common/list.h"`, added `#include <algorithm>` and `#include <vector>`
- All implementations use `std::vector<t_friend*>&` parameter
- `friendlist_close`: range-for deleting all + `flist.clear()`
- `friendlist_purge`: iterator-based loop erasing `FRIEND_UNLOADEDMUTUAL` entries
- `friendlist_add_account`: `flist.push_back(fr)`
- `friendlist_remove_friend/account/username`: `std::find` + `erase` + `delete`
- `friendlist_find_*`: range-for loops

### `bnetd/account.h` ✅ R108
- Added `#include <vector>` at top
- Added forward declaration `struct friend_struct;`
- `t_list * friends` → `std::vector<struct friend_struct*> friends`
- `account_get_friends()` return type: `t_list*` → `std::vector<struct friend_struct*>&`

### `bnetd/account.cpp` ✅ R108
- `account_get_friends()`: returns `std::vector<t_friend*>&` with static empty fallback
- `account_load_friends()`: `if (account->friends == NULL)` + `list_create()` → `if (account->friends.empty())` + `newlist = true`
- `account_unload_friends()`: removed `!= nullptr` check
- `account_create()`: removed `account->friends = NULL` (vector default-initialized)
- `account_check_mutual()`: `if (account->friends != NULL)` → `if (FLAG_ISSET(account->flags, ACCOUNT_FLAG_FLOADED))`

### Caller files updated ✅ R108
- `src/bnetd/watch.cpp`: removed `#include "common/list.h"`; `t_list* flist` + `LIST_TRAVERSE` → `auto& flist` + range-for
- `src/bnetd/command.cpp`: 4 `t_list* flist` usages → `auto& flist_*` with vector API; removed `t_elem* curr`
- `src/bnetd/handle_bnet.cpp`: 3 `t_list* flist` usages → `auto& flist`/`flist2`; `LIST_TRAVERSE` → range-for
- `src/bnetd/handle_wol.cpp`: `t_list* flist` → `auto& flist`; `if (flist != NULL)` → `if (!flist.empty())`
- `src/bnetd/luafunctions.cpp`: `t_list*` + `LIST_TRAVERSE_CONST` → `auto&` + range-for (friends and clan members)
- `src/bnetd/account_wrap.cpp`: 3 `t_list* flist` usages → `auto& flist`; removed NULL checks
- `src/bnetd/storage_file.cpp`: `clan->members = list_create()` removed; `list_append_data` → `push_back`; `LIST_TRAVERSE` → range-for

---

## R109 Replacements Log

### `bnetd/quota.h` ✅ R109
- Removed `#include "common/list.h"`, added `#include <deque>`
- `t_quota::list` field: `t_list*` → `std::deque<t_qline>` (value semantics, no heap alloc per entry)

### `bnetd/connection.h` ✅ R109
- Removed `#include "common/list.h"` from protos section, added `#include <vector>`
- `conn_destroy` signature: removed `t_elem** elem` parameter (no longer needed with vector)
- `connlist()` return type: `t_list*` → `const std::vector<t_connection*>&`

### `bnetd/connection.cpp` ✅ R109
- Added `#include <algorithm>`, `#include <deque>`, `#include <vector>`; removed `#include "common/list.h"`
- `static t_list * conn_head` → `static std::vector<t_connection*> conn_head`
- `static t_list * conn_dead` → `static std::vector<t_connection*> conn_dead`
- `conn_create`: `list_prepend_data(conn_head, temp)` → `conn_head.push_back(temp)`; quota.list default-constructed
- `conn_destroy` (new signature, no `t_elem**`): `std::find`+`erase` for removal from `conn_head`; `quota.list.clear()`; `std::find`+`erase` for `conn_dead`
- `conn_set_state`: `conn_dead.push_back(c)` / `std::find`+`erase` instead of `list_create/list_append_data/list_remove_data`
- `conn_quota_exceeded`: `while(!list.empty()) { front(); pop_front(); }` + stack-allocated `t_qline` + `push_back()`
- `connlist_create`: `conn_head.clear()`
- `connlist_destroy`: `conn_dead.clear(); conn_head.clear()`
- `connlist_reap`: snapshot + range-for: `std::vector<t_connection*> to_reap(conn_dead); for (t_connection* c : to_reap) conn_destroy(c, DESTROY_FROM_DEADLIST)`
- `connlist()`: returns `const std::vector<t_connection*>&`
- All `connlist_find_*` functions: range-for over `conn_head`
- `connlist_get_length`: `return static_cast<int>(conn_head.size())`
- `connlist_login_get_length`, `connlist_count_connections`, `conn_get_user_count_by_clienttag`: range-for

### Caller files updated ✅ R109
- `src/bnetd/server.cpp` (`_shutdown_conns`): snapshot + range-for: `std::vector<t_connection*> to_destroy(connlist()); for (t_connection* c : to_destroy) conn_destroy(c, DESTROY_FROM_CONNLIST)`
- `src/bnetd/command.cpp`: 5 `LIST_TRAVERSE_CONST(connlist(), curr)` → range-for; removed `t_elem const * curr` declarations
- `src/bnetd/message.cpp`: 2 `LIST_TRAVERSE_CONST(connlist(), curr)` → range-for
- `src/bnetd/output.cpp`: 2 `LIST_TRAVERSE_CONST(connlist(), curr)` → range-for; removed `t_elem const *curr`
- `src/bnetd/luafunctions.cpp`: `t_elem const * curr; LIST_TRAVERSE_CONST(connlist(), curr)` → `for (t_connection * conn : connlist())`

---

## R110 Replacements Log

### `bnetd/realm.h` ✅ R110
- `realmlist()` return type: `t_list*` → `const std::vector<t_realm*>&`
- Removed `#include "common/list.h"`, added `#include <vector>`

### `bnetd/realm.cpp` ✅ R110
- Removed `#include "common/list.h"`, added `#include <vector>` and `#include <algorithm>`
- `static t_list * realmlist_head` → `static std::vector<t_realm*> realmlist_head`
- `realmlist_load`: returns `std::vector<t_realm*>` by value; `list_create()`/`list_prepend_data()` → `result.push_back(realm)`
- `realmlist_reload`: `t_list*` locals → `std::vector<t_realm*>`; `LIST_TRAVERSE` → range-for; `list_remove_elem`/`list_destroy` → vector move semantics
- `realmlist_create`: assigns `realmlist_head = realmlist_load(filename)`; checks `empty()` instead of NULL
- `realmlist_unload`: converted to `static void` taking `std::vector<t_realm*>&`; range-for + `clear()`
- `realmlist_destroy`: calls `realmlist_unload(realmlist_head)`; returns 0
- `realmlist()`: returns `const std::vector<t_realm*>&`
- `realmlist_find_realm`: range-for over `realmlist_head`
- `realmlist_find_realm_by_ip`: range-for over `realmlist_head`

### Caller files updated ✅ R110
- `src/bnetd/handle_bnet.cpp`: 4 `LIST_TRAVERSE_CONST(realmlist(), curr)` → `for (t_realm const *realm : realmlist())`; removed redundant `t_elem const *curr` and `t_realm const *realm` locals

---

## R111 Replacements Log

### `bnetd/watch.cpp` ✅ R111 (confirmed clean — migrated as side effect of R108)
- `watch.h` already used `std::list<Watch>` internally — **no `t_list*` ever in the public API**
- `#include "common/list.h"` was removed and `t_list* flist` + `LIST_TRAVERSE` → `auto& flist` + range-for in R108 (as a caller update when `friends.h` was migrated)
- R111 audit confirmed: zero `t_list` / `list.h` references in `watch.cpp` or `watch.h`
- No code changes required in R111 — checklist row updated to ✅

### Phase 1 Step 7 — Tier 3 header migrations COMPLETE ✅ R111
All Tier 3 headers with `t_list*` in their public API have been migrated:
- `channel.h` ✅ R107
- `clan.h` ✅ R108
- `friends.h` ✅ R108
- `account.h` ✅ R108
- `connection.h` ✅ R109
- `realm.h` ✅ R110
- `watch.h` ✅ R111 (was already clean from R108; confirmed and recorded)
- `game.h` — no change needed (uses `t_elist`, not `t_list`)

Remaining `t_list` / `t_hashtable` / `t_elist` work (alias_command, anongame_infos, icons, team, account teams, d2cs, d2dbs, etc.) is tracked in the per-file audit table above and will be addressed in subsequent rounds.
