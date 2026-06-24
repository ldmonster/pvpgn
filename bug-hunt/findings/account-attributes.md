# Bug Hunt: account + attributes subsystem (ORIGINAL vs v3)

Scope: attribute key strings, Record stat key format, profile keys, command-group
bit layout + default, account-creation defaults, win/loss/disc increment semantics.

ORIGINAL (read-only): `/home/cnupt/work/pvpgn-server`
V3 (read-only): `/home/cnupt/work/pvpgn`

Legend: severity = data-corruption impact; classification = BUG / INTENTIONAL / UNSURE.

---

## FINDING 1 — Profile attribute keys renamed (`profile\*` -> `BNET\acct\*`)  [CRITICAL / BUG]

The original profile fields (sex/age/location/description) live under the
`profile\` namespace. The client itself writes them there: `handle_bnet.cpp` only
accepts profile updates whose key begins with `"profile\\"` (see below). v3 reads
and writes them under `BNET\acct\` instead — a different key. Data written by a
real client under `profile\sex` will never be read back by v3, and v3-written
`BNET\acct\sex` is invisible to the original profile path.

Original ref — `src/bnetd/account_wrap.cpp:548,563,578,594`:
```cpp
account_get_strattr(account, "profile\\sex");
account_get_strattr(account, "profile\\age");
account_get_strattr(account, "profile\\location");
account_get_strattr(account, "profile\\description");
```
Original write gate — `src/bnetd/handle_bnet.cpp:3529`:
```cpp
if (std::strlen(key) < 9 || strncasecmp(key, "profile\\", 8) != 0)  // only profile\ keys accepted
```

V3 ref — `src/domain/identity/src/attribute_map.cpp:23-39, 69-79`:
```cpp
std::optional<std::string> AttributeMap::sex() const {
    auto val = get("BNET\\acct\\sex");          // WRONG: original is "profile\\sex"
...
std::optional<std::string> AttributeMap::location() const {
    auto val = get("BNET\\acct\\location");      // WRONG: original is "profile\\location"
...
std::optional<std::string> AttributeMap::description() const {
    auto val = get("BNET\\acct\\description");   // WRONG: original is "profile\\description"
```

Divergence:
- sex: v3 `BNET\acct\sex`  vs  original `profile\sex`
- location: v3 `BNET\acct\location`  vs  original `profile\location`
- description: v3 `BNET\acct\description`  vs  original `profile\description`
- age: v3 has **no** age accessor at all (original `profile\age` is dropped).

Proposed fix: use the `profile\` keys: `profile\\sex`, `profile\\age`,
`profile\\location`, `profile\\description`. Add an `age()` accessor.

---

## FINDING 2 — Record stat key format is wrong: order swapped + numeric tag + missing ladder-id segment  [CRITICAL / BUG]

The original per-client stat key is:
`Record\<TAGSTR>\0\<stat>` for normal stats, where `<TAGSTR>` is the 4-char ASCII
tag (e.g. `WAR3`, `STAR`) produced by `tag_uint_to_str2`, and `0` is the ladder-id
segment (0 = "normal"/non-ladder). v3's `make_stat_key` produces
`Record\<stat>\<numeric-packed-tag>` — the stat name and tag are in the wrong order,
the tag is emitted as a **decimal integer** instead of the 4-char string, and the
`\0\` ladder-id segment is missing entirely.

Original ref — `src/bnetd/account_wrap.cpp:698, 740, 825` (and ~all stat getters):
```cpp
std::string key("Record\\" + tag_uint_to_str2(clienttag) + "\\0\\wins");
std::string key("Record\\" + tag_uint_to_str2(clienttag) + "\\0\\losses");
std::string key("Record\\" + tag_uint_to_str2(clienttag) + "\\0\\disconnects");
```
`tag_uint_to_str2` returns the 4-char ASCII string (`src/common/tag.cpp:171-183`),
so a real key is e.g. `Record\WAR3\0\wins`.

V3 ref — `src/domain/identity/src/attribute_map.cpp:93-97`:
```cpp
static std::string make_stat_key(std::string_view prefix, ClientTag tag) {
    std::ostringstream oss;
    oss << "Record\\" << prefix << "\\" << tag.packed_be();   // e.g. "Record\\wins\\1463898674"
    return oss.str();
}
```
`packed_be()` returns the big-endian uint32 (`client_tag.hpp:82-87`); streaming it
yields a **decimal** number, not `WAR3`.

Divergence (for tag WAR3, win stat):
- original: `Record\WAR3\0\wins`
- v3:       `Record\wins\1463898674`

Three independent errors: (a) `prefix`/`tag` order swapped, (b) tag rendered numeric
not as `tag.text()`, (c) `\0\` ladder-segment dropped.

Proposed fix:
```cpp
oss << "Record\\" << tag.text() << "\\0\\" << prefix;   // Record\WAR3\0\wins
```
For the ladder variants (see Finding 3) the `0` must be replaced by the ladder id.

---

## FINDING 3 — Ladder stat keys: wrong segment + numeric/string ladder-id mismatch  [HIGH / BUG]

v3 models "ladder" wins/losses as `make_stat_key("ladder_wins", tag)` ->
`Record\ladder_wins\<num>`. The original has **no** `ladder_wins` literal. Ladder
record keys are `Record\<TAGSTR>\<ladder-id>\wins` where `<ladder-id>` is either the
numeric id or the ladder name string depending on the accessor:

- numeric id form — `account_get_ladder_active_wins` etc.
  `src/bnetd/account_wrap.cpp:910`:
  ```cpp
  std::string key("Record\\" + tag_uint_to_str2(clienttag) + "\\" + std::to_string(id) + "\\active wins");
  ```
- ladder-name form — `account_get_ladder_wins` etc.
  `src/bnetd/account_wrap.cpp:1108`:
  ```cpp
  std::string key("Record\\" + tag_uint_to_str2(clienttag) + "\\" + ladder_id_str.at((size_t)id) + "\\wins");
  ```
  (`ladder_id_str` maps the id to a name segment — e.g. "solo"/"team"/"ffa".)

V3 ref — `src/domain/identity/src/attribute_map.cpp:129-147,164-172`:
```cpp
get(make_stat_key("ladder_wins", tag));    // "Record\\ladder_wins\\<num>"
get(make_stat_key("ladder_losses", tag));
```

Divergence: v3 has no ladder-id segment at all, collapses solo/team/ffa into a single
`ladder_wins` bucket, and (same as Finding 2) swaps order + numeric tag. Stats will
not interoperate with original storage and conflate distinct ladders.

Proposed fix: model the ladder id explicitly and produce
`Record\<TAGSTR>\<ladder-name>\wins` (matching `account_get_ladder_wins`), with a
separate code path for the numeric-id "active" keys if those are needed.

---

## FINDING 4 — created-at / last-login keys renamed  [HIGH / BUG]

Original account-creation timestamp key is `BNET\acct\ctime`; last-login time is
`BNET\acct\lastlogin_time`. v3 uses `BNET\acct\createtime` and `BNET\acct\lastlogin`.

Original ref:
- `src/bnetd/account.cpp:167` — `account_set_numattr(account, "BNET\\acct\\ctime", (unsigned int)now)`
- `src/bnetd/account_wrap.cpp:607` — `account_get_numattr(account, "BNET\\acct\\ctime")`
- `src/bnetd/account_wrap.cpp:612` — `"BNET\\acct\\lastlogin_time"`

V3 ref — `src/domain/identity/src/attribute_map.cpp:41-63, 81-89`:
```cpp
get("BNET\\acct\\lastlogin");     // WRONG: original is "BNET\\acct\\lastlogin_time"
get("BNET\\acct\\createtime");    // WRONG: original is "BNET\\acct\\ctime"
```

Divergence:
- created: v3 `BNET\acct\createtime`  vs  original `BNET\acct\ctime`
- last-login: v3 `BNET\acct\lastlogin`  vs  original `BNET\acct\lastlogin_time`

Proposed fix: use `BNET\\acct\\ctime` and `BNET\\acct\\lastlogin_time`.
Note also original `lastlogin_time` stores a unix time as a plain number; v3's
parse uses `std::stoll` which is fine, but the key name must match.

---

## FINDING 5 — disconnects increment but never round-trip with `inc_normal_disconnects`  [MEDIUM / consequence of F2]

v3 `increment_disconnects` writes `make_stat_key("disconnects", tag)` =
`Record\disconnects\<num>`; original writes `Record\<TAGSTR>\0\disconnects`
(`account_wrap.cpp:839`). Same root cause as Finding 2. The increment *semantics*
(read current, +1, store) match the original `account_inc_normal_disconnects`
read-modify-write at `account_wrap.cpp:831-842`, so only the KEY is wrong, not the
counter logic. Listed separately because the disconnects key shares all three
defects of Finding 2.

---

## FINDING 6 — command-group storage format differs (bitmask int vs CSV of group numbers)  [MEDIUM / UNSURE]

Original stores command groups as a single integer bitmask under
`BNET\auth\command_groups`, consumed via bit-AND with `command_get_group()` which
returns `1 << (group-1)`.

Original ref:
- `src/bnetd/account_wrap.cpp:2018-2025` — get/set `BNET\auth\command_groups` as a numeric attr.
- `src/bnetd/command_groups.cpp:91` — `entry->group = 1 << (group - 1);`
- usage e.g. `src/bnetd/command.cpp:2999` — `account_get_command_groups(...) & command_get_group("/admin-addr")`.

V3 ref:
- `src/domain/identity/include/domain/identity/account.hpp:39-65` — `CommandGroupMask`
  stores a `std::bitset<8>`, `grant`/`has` use `group-1`. **Bit layout MATCHES**
  (group 1 = bit 0). Good.
- `src/infra/mysql/src/account_repository.cpp:88,194-196` — persists as a
  **comma-separated list of group numbers** ("1,3,7"), not as the integer bitmask.

Divergence: v3 mysql repo serializes groups as CSV text, not as the legacy integer
bitmask. This round-trips internally within v3, but is NOT the original on-disk/DB
representation. Whether this matters depends on whether v3 intends to read legacy
storage. Classified UNSURE because v3 has its own `accounts` table schema; flagging
in case parity with legacy account files / SQL `command_groups` column is intended.
If legacy interop is required, store/parse the integer bitmask.

---

## FINDING 7 — `is_admin()` semantics: group 7/8 vs separate `BNET\auth\admin` flag  [MEDIUM / UNSURE]

V3 `CommandGroupMask::is_admin()` returns true if group 7 or 8 is set
(`account.hpp:56-59`, `bits_.test(6) || bits_.test(7)`).

In the original, "admin" is NOT a command group — it is a distinct boolean attribute
`BNET\auth\admin` (`account_wrap.cpp:238-275, 2009-2016`), independent of the
command-group bitmask. There is no convention that groups 7/8 == admin in the
original; the command_groups.conf shipped maps group numbers to command sets defined
by the operator, with no built-in "group 7/8 = admin" rule.

Divergence: v3 conflates high command groups with admin status. Also, the v3
`InMemoryPermissionChecker` (`permission_checker.cpp:75-79,108-111`) hardcodes
group 1="admin", 2="mod", 3="operator", 4="voice", which is yet a third, different
fixed mapping — and contradicts `is_admin()`'s "group 7/8" rule.

Classified UNSURE: this may be an intentional v3 simplification, but it does not
mirror original semantics and the two v3 mappings (is_admin vs permission_checker)
are mutually inconsistent. Worth confirming intent.

---

## FINDING 8 — account-creation defaults: no default command group (MATCHES)  [INFO]

Original `account_create` (`src/bnetd/account.cpp:120-171`) sets only:
`BNET\acct\username`, `BNET\acct\userid`, `BNET\acct\passhash1`, `BNET\acct\ctime`,
and `FLAG_ZERO(&account->flags)`. It does NOT set a default command group, so the
default is effectively 0 (no groups). v3 `CreateAccount::execute`
(`create_account.cpp`) and `Account::create` (`account.hpp:71-76`) likewise leave
`groups_` default-constructed = empty `bitset` = 0. **Default command group MATCHES
(both = none/0).** No locale/flag default divergence observed in the creation path
(locale comes from the request in both). No bug here.

---

## SUMMARY OF MATCHES (verified correct)

- Command-group **bit layout**: group N -> bit N-1, in both `CommandGroupMask`
  (`account.hpp:45-52`) and original `1 << (group-1)` (`command_groups.cpp:91`). MATCH.
- Default command group on creation: both = 0/none. MATCH.
- `BNET\acct\email` key: v3 (`attribute_map.cpp:17`) == original
  (`account_wrap.cpp:2620`). MATCH.
- `BNET\acct\username` key: v3 (`attribute_map.cpp:11`) == original
  (`account.cpp:152`). MATCH.
- win/loss/disconnect **increment logic** (read current, +1, write back): v3
  `increment_*` matches original `account_inc_*` read-modify-write. Only the KEY
  STRINGS are wrong (Findings 2,3,5), not the counter arithmetic.

---

## PRIORITY

1. Finding 1 (profile keys) — CRITICAL, silent profile data loss vs real clients.
2. Finding 2 (Record stat key format) — CRITICAL, all win/loss/disc stats stored
   under non-interoperable keys; three independent format errors.
3. Finding 3 (ladder keys) — HIGH, ladders conflated + wrong format.
4. Finding 4 (ctime/lastlogin keys) — HIGH, timestamps under wrong keys.
5. Findings 5/6/7 — MEDIUM, follow-on / representation / semantic divergences.
</content>
</invoke>
