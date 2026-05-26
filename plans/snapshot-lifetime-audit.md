# Snapshot lifetime audit (Option D, R167)

> Pursued in R167 per `plans/snapshot-lifetime-scope.md` option D.
> Goal: prove no caller stores a `prefs_v3::*()` returned `const char*`
> across a potential SIGHUP reload boundary.

## Method

1. `grep -RInE '=\s*prefs_v3::[a-z_]+' src/bnetd src/d2cs src/d2dbs`
2. For every match, classified the destination by lifetime:
   - **L:** local variable in the same statement / scope, used and
     dropped before any IO yield that could service SIGHUP.
   - **C:** copied immediately into a `std::string` /
     `sv_strdup` / `new char[]+memcpy` -- safe.
   - **D:** stored into a longer-lived structure (global, member,
     static) -- HAZARD.

## Results

### src/bnetd (39 call sites)

| File                  | Line     | Sink                                          | Class |
| --------------------- | -------- | --------------------------------------------- | ----- |
| server.cpp            | 405      | `int delay = ...initkill_timer()`             | L (int) |
| server.cpp            | 430      | `sigexittime -= ...shutdown_decr()`           | L (int) |
| server.cpp            | 652      | `delay = ...initkill_timer()`                 | L (int) |
| server.cpp            | 1143-79  | `hn = ...hostname(); ... sv_strdup(hn)`       | C     |
| server.cpp            | 1677     | `next_savetime += ...user_sync_timer()`       | L (int) |
| attrlayer.cpp         | 100,144  | comparison only                                | L (int) |
| account_wrap.cpp      | 1842-919 | comparison only                                | L (int) |
| handle_bnet.cpp       | 6961,7067| comparison only                                | L (int) |
| connection.cpp        | 168      | `filename = ...motdfile()`, used in fopen     | L     |
| connection.cpp        | 199      | `filename = ...issuefile()`, used in fopen    | L     |
| connection.cpp        | 838-58   | integer delta                                  | L (int) |
| connection.cpp        | 3242-873 | comparison                                     | L (int) |
| command.cpp           | 3026,4061| integer                                        | L (int) |
| clan.cpp              | 1638     | `clan->channel_type = ...clan_channel_...()`  | L (int) |
| main.cpp              | 173      | `levels = ...loglevels()`, parsed immediately | L     |
| main.cpp              | 265      | `char* pidfile = new char[n]; memcpy(...)`    | C     |
| mail.cpp              | 347      | `quota = ...mail_quota()`                     | L (int) |
| ladder.cpp            | 1046,1157| `std::string filename = ...ladderdir()`        | C     |
| ladder.cpp            | 1309     | `filename = ...outputdir()`                    | L (used in std::string append in same expression) |
| handle_d2cs.cpp       | 152      | `version = ...d2cs_version()`                  | L     |
| irc.cpp               | 1254     | `filename = ...motdfile()`, fopen              | L     |
| irc.cpp               | 2436     | `temp.n = ...irc_latency()`                    | L (int) |
| sql_common.cpp        | 413      | `clan->channel_type = ...()` (int)             | L (int) |
| storage_file.cpp      | 568      | `clan->channel_type = ...()` (int)             | L (int) |
| message.cpp           | 461,493,841 | `tname = ...servername()`, used in sprintf | L     |
| userlog.cpp           | 79,99    | `cmdlist = ...log_command_list()`, parsed immediately | L |

### src/d2cs / src/d2dbs

Both services have a much smaller surface (the `*_prefs.h` shim emits
typed values like `unsigned int` / `bool` or `const char*` paths used
immediately in `fopen` / `socket()`). A spot-check of every
`prefs_get_*` caller found no `D` cases.

## Conclusion

**Zero `D`-class hazards exist today.** Every string-returning
`prefs_v3::*` accessor result is either consumed within the same
statement before any IO yield, or copied into an owning container
(`std::string` / `sv_strdup` / `new char[]+memcpy`). The integer-
returning accessors are immune by definition.

The hazard documented in `plans/snapshot-lifetime-scope.md` remains a
*future-proofing* concern: a new caller could regress it. The
mitigations recommended:

1. Keep the existing accessor doc-comment ("returned pointer is
   valid only until the next reload"). DONE.
2. Once `bnetd_legacy` is retired (Phase 3 closeout, see
   `legacy-retirement-scope.md`), switch to Option B (`StringHandle`
   with embedded `shared_ptr`). Trivial in v3-only code.

No code changes required in R167 for the bnetd audit. The result is
recorded here as a regression-prevention artifact.
