# Bug Hunt — Flat-file account storage: can v3 read a REAL legacy `.plain` file?

**Verdict: NO.** v3's flat-file reader cannot parse a real legacy PvPGN `.plain`
account file. Every account read from legacy data resolves *all* fields as empty,
including the required `username`, so v3 silently treats real legacy accounts as
absent/blank. This breaks both `pvpgn-migrate` (imports nothing / blank) and any
file-backed runtime account load. v3 is self-consistent (it reads what it writes),
but its on-disk format is **incompatible** with the original in two independent
ways: key/value quoting and backslash escaping.

---

## The legacy on-disk format (ORIGINAL — ground truth)

The original writes every attribute with **double-quoted, escaped** key and value:

- Writer: `src/bnetd/file_plain.cpp:88`
  ```c
  std::fprintf(accountfile, "\"%s\"=\"%s\"\n", key, val);
  ```
  Both `key` and `val` are first passed through `escape_chars()`.
- Escape rule: `src/common/util.cpp:352` (`escape_chars`) doubles every backslash
  (`\` → `\\`) and escapes quotes (`"` → `\"`), among others (lines 364-373).
- Reader: `src/bnetd/file_plain.cpp:137`
  ```c
  std::sscanf(buff, "\"%[^\"]\" = \"%[^\"]\"", esckey, escval)
  ```
  i.e. it expects surrounding double-quotes, then `unescape_chars()`
  (`src/common/util.cpp:426`) collapses `\\` → `\` and `\"` → `"`.

Internal attribute keys use a single backslash as separator, e.g.
`BNET\acct\username` (see `src/bnetd/storage.h` / account attribute keys).

**Therefore the exact bytes on disk for account "Joe" are:**

```
"BNET\\acct\\username"="Joe"
"BNET\\acct\\passhash1"="<40 hex>"
"BNET\\acct\\userid"="2"
```

Note: surrounding `"` quotes **and** doubled `\\` separators. That is what a real
legacy server, and `pvpgn-migrate` consuming legacy data, will encounter.

---

## What v3 actually parses

- Reader: `src/infra/file/src/flat_db_reader.cpp`
  - `parse_account_file` (lines 9-42): splits each line on the **first `=`**,
    takes `line.substr(0, eq_pos)` as the raw key and the remainder as the raw
    value. It trims **only** a single trailing space from the key and a single
    leading space from the value (lines 31-36). It does **not** strip surrounding
    double-quotes, and does **not** unescape `\\`/`\"`.
  - `get_field` (lines 44-58): rebuilds the lookup key by joining the path
    components with a **single** backslash (line 51: `key_oss << "\\";`), giving
    `BNET\acct\username` (unquoted, single backslash), then does an exact
    `data.find(...)`.

So v3 looks up the map key `BNET\acct\username`.

---

## Why the lookup fails (concrete round-trip)

Given the real legacy line:

```
"BNET\\acct\\username"="Joe"
```

`parse_account_file` splits on the first `=` →
- key (map) = `"BNET\\acct\\username"`  (literally: quote, B, N, E, T, backslash,
  backslash, a, c, c, t, backslash, backslash, ..., quote)
- value     = `"Joe"`  (with the surrounding quotes still attached)

`get_field({"BNET","acct","username"})` searches for `BNET\acct\username`
(no quotes, single backslashes). **No match** → returns `""`.

Result in `FileAccountRepository::load_account_file`
(`src/infra/file/src/account_repository.cpp:128-131`): `username_str.empty()` is
true → returns `std::nullopt`. The account is dropped during `load_all()`
(lines 96-101, the `nullopt` is discarded). Same control flow in
`pvpgn-migrate` (`src/app/pvpgn-migrate/migrate_accounts.cpp:79-84`): empty
username → account skipped. Even if username somehow matched, every value would
carry stray surrounding quotes (e.g. `passhash1` = `"<hex>"`, fails the 40-char
hex check at `account_repository.cpp:38`; numeric fields would `std::stoll`-fail
and return 0).

---

## What v3 writes (self-consistent, but incompatible)

- Writer: `FileAccountRepository::save`
  (`src/infra/file/src/account_repository.cpp:200-213`) emits, e.g.:
  ```
  BNET\acct\username=Joe
  BNET\acct\passhash1=<40 hex>
  BNET\acct\userid=2
  ```
  Unquoted keys, **single** backslash, unquoted values, no escaping.

This is exactly what v3's own reader expects, so v3 round-trips its own files
(confirmed by the only fixtures used in
`tests/unit/infra/file/file_account_repository_test.cpp`, which write via `save`
then read back — they never test a real legacy line). But:
- The **original** server's reader (`file_plain.cpp:137`, requires leading `"`)
  will reject every v3-written line as "malformed entry" → original cannot read
  v3 files either.
- v3 cannot read original files (this finding).

The two formats are mutually unreadable.

---

## Findings

### F1 — v3 reader does not strip quotes; cannot match legacy keys/values
- **Severity:** CRITICAL
- **Classification:** BUG
- **Original ref:** `src/bnetd/file_plain.cpp:88` (writes `"%s"="%s"`),
  `:137` (reads with leading/trailing `"`); `src/common/util.cpp:352` (escape).
- **v3 ref:** `src/infra/file/src/flat_db_reader.cpp:22-38` (`parse_account_file`
  keeps quotes), `:51` (`get_field` joins with single `\`).
- **Failing line:** `"BNET\\acct\\username"="Joe"` → `get_field` returns `""`.
- **Impact:** required `username` empty → account dropped on load
  (`account_repository.cpp:129-131`) and on migrate
  (`migrate_accounts.cpp:81-84`). Migration silently imports zero/blank accounts;
  file-backed runtime sees no legacy users.

### F2 — v3 reader does not un-double backslashes (`\\` → `\`)
- **Severity:** CRITICAL (compounds F1 — independent second cause)
- **Classification:** BUG
- **Original ref:** `src/common/util.cpp:364-368` doubles `\`;
  `:446-448` (`unescape_chars`) collapses it back on read.
- **v3 ref:** `flat_db_reader.cpp` has **no** unescape step; `get_field` builds a
  single-backslash key (`:51`). Even if quotes were stripped, the stored key
  `BNET\\acct\\username` would still not equal the lookup `BNET\acct\username`.
- **Failing line:** same as F1.
- **Note:** also means values containing escaped chars (e.g. embedded quotes in
  description fields) would be corrupted.

### F3 — v3 writer emits a format the original cannot read
- **Severity:** HIGH
- **Classification:** BUG (compatibility) — likely UNSURE whether intentional,
  but no code or comment indicates a deliberate format break.
- **Original ref:** `src/bnetd/file_plain.cpp:137` requires leading `"`.
- **v3 ref:** `account_repository.cpp:200-213` writes unquoted single-backslash.
- **Example:** v3 writes `BNET\acct\username=Joe`; original `sscanf` at `:137`
  and the empty-value fallback at `:138` both fail → "malformed entry", line
  skipped. A site running v3 cannot roll back to the original on its data.

### F4 — Only a 6-field subset is written; legacy data is lossy even if formats matched
- **Severity:** MEDIUM (out of scope of the quoting bug but worth flagging)
- **Classification:** UNSURE (may be intentional MVP scope)
- **v3 ref:** `account_repository.cpp:200-213` writes only username, passhash1,
  auth_lock, auth_command_groups, locale, userid. Ban, must-change-password,
  email, profile, per-game stats, character data, etc. are not persisted
  (acknowledged in comments at `:177-179`). Even a corrected reader would lose
  all other legacy attributes on a save round-trip.

---

## What MATCHES

- The **logical key path** is correct: both sides use the
  `BNET\acct\<field>` namespace and the same field names
  (`username`, `passhash1`, `userid`, `auth_lock`, `auth_command_groups`,
  `locale`). The only mismatch is the *physical* line syntax (quoting + escaping),
  not the field naming.
- The `passhash1` is a 40-char hex BNet hash on both sides
  (`account_repository.cpp:37-57` vs original storage).
- v3 is internally consistent: `save` output is readable by `parse_account_file`.

---

## Proposed fix (single, localized — in `parse_account_file`)

Make the reader accept the legacy syntax (and stay backward-compatible with v3's
own unquoted output) by, per line:
1. If the key segment is wrapped in `"..."`, strip the surrounding quotes; same
   for the value.
2. Run an unescape pass over the de-quoted key and value matching
   `unescape_chars` (`\\`→`\`, `\"`→`"`, `\n`,`\t`, octal `\ooo`, ...).

This produces map key `BNET\acct\username` and value `Joe`, which `get_field`
(single-backslash join) then matches unchanged. No change needed in `get_field`.

For full bidirectional compatibility, the **writer**
(`account_repository.cpp:200-213`) should also emit the original
`"<escaped-key>"="<escaped-val>"` format (mirror of `escape_chars`), so that v3
files are readable by the original and by the (fixed) v3 reader. Add a test
fixture containing a verbatim real legacy line
(`"BNET\\acct\\username"="Joe"`) — the current tests only round-trip v3's own
output and would not catch this.

---

## Confidence

High. The mismatch is provable by static reading of both writers and both
readers; no build needed. The original writes quotes + doubled backslashes
(`file_plain.cpp:88` + `util.cpp:364`), and v3's `parse_account_file` does
neither strip nor unescape (`flat_db_reader.cpp:22-38`). The prior pass's note is
**confirmed and is more severe than "fields read as empty": the required
username read fails, so whole accounts are dropped/blank-imported.**
