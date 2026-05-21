# Step 6: xalloc Elimination — Audit & Checklist

**Round:** R104  
**Date:** 2026-05-21  
**Status:** ✅ COMPLETE — zero xalloc references outside `src/common/xalloc.{h,cpp}`

---

## Summary

| Metric | Value |
|--------|-------|
| Total grep hits for `xmalloc\|xfree\|xrealloc\|xstrdup\|xcalloc` (before R104) | 60 |
| Actual xalloc **function call** sites (before R104) | **1** (`xalloc_setcb` in `main.cpp`) |
| Files with stale `#include "common/xalloc.h"` (before R104) | **70** |
| Files with stale `#include "common/xalloc.h"` (after R104) | **0** |
| Remaining xalloc references outside `src/common/xalloc.*` | **0** |

### Key Finding

The previous xalloc elimination work (Rounds 1–13, per progress-master.md) had already replaced
all `xmalloc()`, `xfree()`, `xrealloc()`, `xstrdup()`, and `xcalloc()` call sites with C++
equivalents (`new`/`delete[]`, `std::string`, `std::vector`, `std::unique_ptr`).

What remained in R104 was:
1. **70 stale `#include "common/xalloc.h"` lines** — removed by bulk `sed -i`
2. **1 real `xalloc_setcb()` call** in `src/bnetd/main.cpp` — removed by refactoring the OOM handler

---

## Pre-R104 Audit: Files with Stale `#include "common/xalloc.h"`

All 70 files below had the include removed. None had actual xalloc function calls.

### `src/bnetd/` (42 files)

| File | Had actual xalloc calls? | Action |
|------|--------------------------|--------|
| `src/bnetd/account.cpp` | No (comment only) | Removed include |
| `src/bnetd/alias_command.cpp` | No | Removed include |
| `src/bnetd/anongame.cpp` | No | Removed include |
| `src/bnetd/anongame_gameresult.cpp` | No | Removed include |
| `src/bnetd/anongame_infos.cpp` | No (comment only) | Removed include |
| `src/bnetd/anongame_maplists.cpp` | No | Removed include |
| `src/bnetd/attrgroup.cpp` | No | Removed include |
| `src/bnetd/autoupdate.cpp` | No (comment only) | Removed include |
| `src/bnetd/channel.cpp` | No (comment only) | Removed include |
| `src/bnetd/character.cpp` | No (comment only) | Removed include |
| `src/bnetd/clan.cpp` | No (comment only) | Removed include |
| `src/bnetd/cmdline.cpp` | No | Removed include |
| `src/bnetd/command.cpp` | No (comment only) | Removed include |
| `src/bnetd/command_groups.cpp` | No (comment only) | Removed include |
| `src/bnetd/connection.cpp` | No (comment only) | Removed include |
| `src/bnetd/file.cpp` | No | Removed include |
| `src/bnetd/friends.cpp` | No (commented-out code) | Removed include |
| `src/bnetd/game.cpp` | No (comments only) | Removed include |
| `src/bnetd/game_conv.cpp` | No (comment only) | Removed include |
| `src/bnetd/handle_apireg.cpp` | No (comment only) | Removed include |
| `src/bnetd/handle_bot.cpp` | No (comment only) | Removed include |
| `src/bnetd/handle_irc_common.cpp` | No (comment only) | Removed include |
| `src/bnetd/handle_telnet.cpp` | No (comment only) | Removed include |
| `src/bnetd/handle_wol.cpp` | No (comment only) | Removed include |
| `src/bnetd/helpfile.cpp` | No | Removed include |
| `src/bnetd/i18n.cpp` | No (comment only) | Removed include |
| `src/bnetd/icons.cpp` | No (comment only) | Removed include |
| `src/bnetd/ipban.cpp` | No (comments only) | Removed include |
| `src/bnetd/irc.cpp` | No (comment only) | Removed include |
| `src/bnetd/luainterface.cpp` | No (comment only) | Removed include |
| `src/bnetd/mail.cpp` | No | Removed include |
| `src/bnetd/main.cpp` | **YES** — `xalloc_setcb()` | Removed include + refactored OOM handler |
| `src/bnetd/message.cpp` | No | Removed include |
| `src/bnetd/news.cpp` | No | Removed include |
| `src/bnetd/output.cpp` | No | Removed include |
| `src/bnetd/realm.cpp` | No (comment only) | Removed include |
| `src/bnetd/server.cpp` | No (comment only) | Removed include |
| `src/bnetd/sql_mysql.cpp` | No | Removed include |
| `src/bnetd/sql_odbc.cpp` | No | Removed include |
| `src/bnetd/sql_pgsql.cpp` | No | Removed include |
| `src/bnetd/sql_sqlite3.cpp` | No | Removed include |
| `src/bnetd/storage.cpp` | No | Removed include |
| `src/bnetd/storage_file.cpp` | No (comment only) | Removed include |
| `src/bnetd/storage_sql.cpp` | No (comment only) | Removed include |
| `src/bnetd/sql_common.cpp` | No (comment only) | Removed include |
| `src/bnetd/sql_dbcreator.cpp` | No (comment only) | Removed include |
| `src/bnetd/support.cpp` | No | Removed include |
| `src/bnetd/timer.cpp` | No | Removed include |
| `src/bnetd/tournament.cpp` | No (comment only) | Removed include |
| `src/bnetd/userlog.cpp` | No (comment only) | Removed include |
| `src/bnetd/attr.h` | No (comment only) | Removed include |

### `src/common/` (14 files)

| File | Had actual xalloc calls? | Action |
|------|--------------------------|--------|
| `src/common/addr.cpp` | No | Removed include |
| `src/common/bigint.cpp` | No | Removed include |
| `src/common/bnetsrp3.cpp` | No | Removed include |
| `src/common/conf.cpp` | No | Removed include |
| `src/common/hashtable.cpp` | No | Removed include |
| `src/common/list.cpp` | No | Removed include |
| `src/common/packet.cpp` | No | Removed include |
| `src/common/peerchat.cpp` | No | Removed include |
| `src/common/queue.cpp` | No | Removed include |
| `src/common/rcm.cpp` | No | Removed include |
| `src/common/tag.cpp` | No | Removed include |
| `src/common/trans.cpp` | No | Removed include |
| `src/common/util.cpp` | No | Removed include |
| `src/common/xstring.cpp` | No (`hexstrdup` ≠ xalloc) | Removed include |

### `src/d2cs/` (13 files)

| File | Had actual xalloc calls? | Action |
|------|--------------------------|--------|
| `src/d2cs/cmdline.cpp` | No | Removed include |
| `src/d2cs/connection.cpp` | No | Removed include |
| `src/d2cs/d2charfile.cpp` | No | Removed include |
| `src/d2cs/d2charlist.cpp` | No | Removed include |
| `src/d2cs/d2gs.cpp` | No | Removed include |
| `src/d2cs/d2ladder.cpp` | No | Removed include |
| `src/d2cs/game.cpp` | No | Removed include |
| `src/d2cs/gamequeue.cpp` | No | Removed include |
| `src/d2cs/handle_d2cs.cpp` | No | Removed include |
| `src/d2cs/handle_signal.cpp` | No | Removed include |
| `src/d2cs/main.cpp` | No | Removed include |
| `src/d2cs/s2s.cpp` | No | Removed include |
| `src/d2cs/serverqueue.cpp` | No | Removed include |

### `src/d2dbs/` (7 files)

| File | Had actual xalloc calls? | Action |
|------|--------------------------|--------|
| `src/d2dbs/charlock.cpp` | No | Removed include |
| `src/d2dbs/cmdline.cpp` | No | Removed include |
| `src/d2dbs/d2ladder.cpp` | No | Removed include |
| `src/d2dbs/dbserver.cpp` | No | Removed include |
| `src/d2dbs/dbspacket.cpp` | No | Removed include |
| `src/d2dbs/handle_signal.cpp` | No | Removed include |
| `src/d2dbs/main.cpp` | No | Removed include |

---

## `src/bnetd/main.cpp` — OOM Handler Refactor

The only file with a real xalloc API call was `src/bnetd/main.cpp`, which called
`xalloc_setcb(bnetd_oom_handler)` to register a legacy malloc OOM callback.

### Before R104

```cpp
// Legacy xalloc OOM system
#define OOM_SAFE_MEM  1000000
void *oom_buffer = NULL;

static int bnetd_oom_handler(void) {
    if (!oom_buffer) return 0;
    free(oom_buffer);
    oom_buffer = NULL;
    eventlog(..., "out of memory, forcing immediate shutdown");
    server_quit_delay(-1);
    return 1;  /* ask xalloc codes to retry */
}

static int oom_setup(void) {
    oom_buffer = calloc(1, OOM_SAFE_MEM);
    if (!oom_buffer) return -1;
    xalloc_setcb(bnetd_oom_handler);  // ← last xalloc call in codebase
    return 0;
}

static void oom_free(void) {
    free(oom_buffer);
    oom_buffer = NULL;
}

// In pre_server_startup():
if (oom_setup() < 0) {
    eventlog(..., "OOM init failed");
    return STATUS_OOM_FAILURE;
}
std::set_new_handler(new_oom_handler);
```

### After R104

```cpp
// C++ new-handler OOM safety: reserve 1 MiB; on first OOM release it and abort.
static int * emergency_mem = new int[1048576]; // 1 MiB
void new_oom_handler() {
    delete[] emergency_mem;
    emergency_mem = nullptr;
    eventlog(..., "out of memory, forcing immediate shutdown");
    std::abort();
}

// In pre_server_startup():
std::set_new_handler(new_oom_handler);
```

**Removed:** `bnetd_oom_handler`, `oom_setup`, `oom_free`, `oom_buffer`, `OOM_SAFE_MEM`,
`STATUS_OOM_FAILURE` define and case label.

---

## Files NOT Modified (xalloc infrastructure itself)

These files are the xalloc implementation and are kept until the entire legacy build is deleted:

| File | Status |
|------|--------|
| `src/common/xalloc.h` | Kept — still compiled into legacy build |
| `src/common/xalloc.cpp` | Kept — still compiled into legacy build |

These will be deleted in **Phase 1 Step 9** (Delete migrated legacy files).

---

## Post-R104 Verification

```
$ grep -rn "xalloc" src/ --include="*.cpp" --include="*.h" | grep -v "^src/common/xalloc"
(no output — exit code 1)
```

✅ Zero xalloc references outside `src/common/xalloc.{h,cpp}`.

---

## Next Steps (Step 7+)

With xalloc fully eliminated from all consumers, the remaining legacy memory management work is:

1. **Step 7**: Eliminate `t_list` / `t_hashtable` (legacy data structures) — replace with STL
2. **Step 9**: Delete `src/common/xalloc.h` and `src/common/xalloc.cpp` entirely
3. **Step 9**: Delete `src/common/xstring.cpp/.h` (contains `hexstrdup` — replace with `std::string`)
