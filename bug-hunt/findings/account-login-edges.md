# Bug Hunt: Account Creation / UID Allocation / Login Edge Cases

ORIGINAL: `/home/cnupt/work/pvpgn-server`
CURRENT v3: `/home/cnupt/work/pvpgn`

Subsystem: account creation, UID allocation, login edge cases.

Legend: classification ∈ {BUG, INTENTIONAL, UNSURE, NOT-IMPLEMENTED}.

---

## FINDING 1 — UID allocation: v3 uses a username HASH, not sequential next-uid (no reuse guarantee, collisions possible)

- **Severity:** HIGH
- **Classification:** BUG
- **Original ref:** `src/bnetd/account.cpp:157`
  ```cpp
  if (account_set_numattr(account, "BNET\\acct\\userid", maxuserid + 1) < 0) {
  ```
  plus `account.cpp:69` `unsigned int maxuserid = 0;` and `account.cpp:686-687`
  ```cpp
  if (uid>maxuserid)
      maxuserid = uid;
  ```
  `maxuserid` is seeded from storage at load (`account.cpp:430 maxuserid = storage->read_maxuserid();`). New uid = `maxuserid + 1`: **strictly sequential, monotonic, never reused.** First-ever account therefore gets uid 1.
- **v3 ref:** `src/application/auth/src/create_account.cpp:25-29`
  ```cpp
  // ID generation: use a simple hash of the canonical username ...
  // In production, this would use a proper ID generator.
  std::string canonical_str(req.username.canonical());
  unsigned int id_value = std::hash<std::string>{}(canonical_str) & 0x7FFFFFFFu;
  auto new_id = domain::AccountId(id_value);
  ```
- **Divergence:**
  - IDs are a 31-bit hash of the username, not sequential. The very first account does **not** get uid 1 (it gets some arbitrary large hash) — this breaks any "first account = uid 1" assumption (see Finding 2).
  - `std::hash` is unspecified/implementation-defined and may differ between libstdc++ versions/builds, so the persisted uid is not stable across toolchains.
  - **Collision risk:** two different usernames can hash to the same 31-bit value. On collision the second `Account::create` produces a duplicate `AccountId`. The SQL repo's `save` is keyed on `id` with `ON CONFLICT(id) DO UPDATE` (`account_repository.cpp:190`), so a colliding second account would **overwrite the first account's row in place** (name/password/etc.), i.e. silent account takeover. The in-memory repo (`infra/inmemory/.../account_repository.hpp:60-61`) likewise stores `by_id_[id] = ...`, clobbering. The username-taken check at `create_account.cpp:19` does not catch this because the two usernames are *different*.
  - The author comment admits this is a placeholder ("In production, this would use a proper ID generator").
- **Proposed fix:** Introduce an `IAccountIdAllocator` port that returns a monotonically increasing id seeded from `max(existing uid)` at startup (mirrors `read_maxuserid` + `maxuserid+1`), persisted/atomic. Never derive the id from the username. Until then, at minimum reject a `save` whose id already maps to a *different* canonical name.

---

## FINDING 2 — First account is NOT special-cased as admin (matches THIS fork's original, but worth noting)

- **Severity:** LOW
- **Classification:** INTENTIONAL (matches original fork) — but verify against deployment expectations
- **Original ref:** In this fork, admin is purely attribute-driven. New accounts are created with `attrgroup_create_newuser` (`account.cpp:144`) which seeds defaults from `conf/bnetd_default_user.plain.in`; there `BNET\auth\admin` defaults to `false` and `BNET\auth\command_groups` = `1`. `account_get_auth_admin` (`account_wrap.cpp:238`) just reads the bool attr with no `uid==1` fallback. Grepping the original (`account.cpp`, `account_wrap.cpp`, `handle_bnet.cpp`) found **no** `uid==1`/"first account" admin special-case. (Classic upstream PvPGN historically defaulted admin for uid 1; this fork does not.)
- **v3 ref:** `domain/identity/account.hpp:57-59` `CommandGroupMask::is_admin()` returns true only if group 7 or 8 is set. `CreateAccount::execute` (`create_account.cpp`) creates the account via `Account::create` with **no** command groups granted, so a fresh account is never admin. No first-account / uid-based admin elevation anywhere.
- **Divergence:** None functionally vs this fork's original (both: first account is not auto-admin). Note v3's admin model is command-group bits 7/8 (`is_admin`), not the `BNET\auth\admin` bool attribute — a different model, but the "first account" behavior matches.
- **Proposed fix:** None required for parity with this fork. If the intent is classic-PvPGN "first account becomes admin", that is missing in BOTH and would need to be added deliberately.

---

## FINDING 3 — Failed-login lockout (passfail_count / passfail_bantime) is NOT IMPLEMENTED

- **Severity:** HIGH
- **Classification:** NOT-IMPLEMENTED
- **Original ref:** `src/bnetd/connection.cpp:3801-3818`
  ```cpp
  extern int conn_increment_passfail_count(t_connection * c) {
      if (prefs_get_passfail_count() > 0) {
          count = conn_get_passfail_count(c) + 1;
          if (count == prefs_get_passfail_count()) {
              ipbanlist_add(NULL, addr_num_to_ip_str(conn_get_addr(c)),
                            now + (std::time_t)prefs_get_passfail_bantime());
              ... conn_set_state(c, conn_state_destroy); return -1;
          }
          else conn_set_passfail_count(c, count);
      }
      return 0;
  }
  ```
  Called on every wrong password, e.g. `handle_bnet.cpp:1649`, `:963`, `:1815`, `:2132`, `:2266`. After `passfail_count` consecutive failures the **source IP** is temp-banned for `passfail_bantime` seconds and the connection is destroyed. Defaults: `passfail_count = 0` (disabled), `passfail_bantime = 300` (`prefs.cpp:3262-3285`, `conf/bnetd.conf.in:365,368`).
- **v3 ref:**
  - `LoginUser::execute` (`application/auth/src/login_user.cpp:29-48`) maps a wrong password to `LoginError::InvalidCredentials` and returns. There is **no** counter increment, no IP-ban-on-N-fails, no connection teardown.
  - `domain/identity/account.hpp` `login()` (`:142-166`) has no failed-attempt counter field at all.
  - Config IS parsed: `infra/config/server_config.hpp:179-180` defines `passfail_count = 0` and `passfail_bantime = 300` (default **correct**, matches original), wired in `server_config.cpp:256-257`. But `grep` across `src/**` shows these fields are **never read** outside the config struct itself — dead config.
- **Divergence:** The whole brute-force lockout feature is absent. (Side note: the task brief mentioned "passfail_bantime default was wrong/0" — in v3 the default is 300, which is correct; the *count* default is 0 = feature off, also matching original. So the default is fine; it's the *enforcement* that is missing.)
- **Proposed fix:** Add a per-connection (or per-IP) consecutive-failure counter in the bnet adapter; on `LoginError::InvalidCredentials` increment it, and when it reaches `policy.passfail_count` (>0), add the peer IP to the IP-ban repo until `now + passfail_bantime` and drop the connection. Wire `policy.passfail_count` / `policy.passfail_bantime` into that path.

---

## FINDING 4 — Already-logged-in: v3 ALWAYS rejects; original's kick_old_login (kick old, admit new) is NOT IMPLEMENTED

- **Severity:** MEDIUM
- **Classification:** NOT-IMPLEMENTED (partial: only the reject branch exists)
- **Original ref:** Two modes selected by `prefs_get_kick_old_login()`:
  - Reject new login when already logged in AND kick disabled — `handle_bnet.cpp:1591`
    ```cpp
    if (connlist_find_connection_by_account(account) && prefs_get_kick_old_login() == 0) {
        ... refused (already logged in) ...
    }
    ```
    (also `:1746`, `:1903` for other login packet types.)
  - Kick old session, admit new — when `kick_old_login != 0` the guard is skipped and `conn_login` → `conn_set_account` (`connection.cpp:1444-1448`) force-destroys the previous connection:
    ```cpp
    if ((other = connlist_find_connection_by_accountname(...))) {
        ... "forcing logout of previous login" ...
        conn_set_state(other, conn_state_destroy);
    }
    ```
- **v3 ref:** `login_user.cpp:62-64`
  ```cpp
  if (auto status = sessions_.attach(req.session, account.id()); !status) {
      return core::fail(LoginError::AlreadyLoggedIn);
  }
  ```
  `InMemorySessionRegistry::attach` (`infra/inmemory/.../session_registry.hpp:28-32`) fails if `session_for_account_` already contains the account. So v3 unconditionally **rejects** the new login — equivalent only to the original's `kick_old_login == 0` case.
- **Divergence:** v3 has no "kick the old session and let the new one in" path. The config flag `kick_old_login` (default `true`, `server_config.hpp:163`) is parsed but **never consumed** (grep finds no reader). So with the default config, original would *kick* the old session, whereas v3 *rejects* the new login — a behavior inversion under default settings.
- **Proposed fix:** When `policy.kick_old_login` is true and `attach` reports the account already has a session, look up the existing session, detach/destroy it (force-logout the old connection), then retry `attach`. Honor the config flag.

---

## FINDING 5 — max_accounts limit NOT enforced on account creation

- **Severity:** MEDIUM
- **Classification:** NOT-IMPLEMENTED
- **Original ref:** `account.cpp:591-603` `accountlist_allow_add()` returns 0 when `prefs_get_max_accounts() <= hashtable_get_length(accountlist_head)` (0 = unlimited). Enforced in `accountlist_add_account` (`account.cpp:628`):
  ```cpp
  if (!accountlist_allow_add()) { ... "account limit reached" ... return NULL; }
  ```
- **v3 ref:** `create_account.cpp:8-49` `CreateAccount::execute` checks IP ban and username-taken, then creates and saves. It never consults `accounts_.size()` against a max. A `ServerFull` error variant exists (`create_account.hpp:45`) but is **never returned**. Config `policy.max_accounts` (`server_config.hpp:162`, default 0) is parsed (`server_config.cpp:239`) but never read by the create path.
- **Divergence:** No upper bound on account count; the wired-but-unused `ServerFull` error and `max_accounts` config confirm this was intended but left unimplemented.
- **Proposed fix:** In `CreateAccount::execute`, after the username-taken check, if `policy.max_accounts > 0 && accounts_.size() >= policy.max_accounts` return `core::fail(CreateAccountError::ServerFull)`. (Requires threading the policy/limit into the use case.)

---

## FINDING 6 — Case-insensitive duplicate rejection & login lookup — MATCHES (verified correct)

- **Severity:** N/A (positive finding)
- **Classification:** INTENTIONAL / correct
- **Original:** lookups are case-insensitive via `account_hash` (lowercases each char, `account.cpp:81-102`) + `strcasecmp` (`account.cpp:238, 664`). Duplicate creation rejected in `account_create` (`account.cpp:139` `accountlist_find_account(username)`) and again in `accountlist_add_account` (`account.cpp:659-670`, `strcasecmp`).
- **v3:**
  - `UserName` stores a lowercased `canonical_` (`domain/shared/user_name.hpp:73,77-78`) and compares by canonical (`:66`).
  - Duplicate check: `create_account.cpp:19` `accounts_.find_by_name(req.username)`.
  - In-memory repo keys by canonical (`infra/inmemory/.../account_repository.hpp:41-42,59-60`) → "Bob" and "bob" collide correctly.
  - SQL repo: `find_by_name` uses `WHERE name = ? COLLATE NOCASE` binding `name.canonical()` (`account_repository.cpp:130-132`); `find_by_id`/login both go through canonical. Case-insensitive lookup confirmed.
- **Verdict:** Duplicate-name rejection (Bob == bob) and case-insensitive login lookup are correctly implemented in v3. **MATCHES original.**
- **Minor note (LOW, not a parity bug):** `SqlAccountRepository::save` builds SQL by raw string concatenation of `account.name().display()`, `locale().text()`, and the password hex (`account_repository.cpp:182-189`) rather than bound parameters. Username chars are validated upstream so injection via name is unlikely, but this is fragile if validation ever loosens (e.g. a `'` in a name would corrupt the statement). Recommend parameterized binds. Also note `save` stores the **display** name while lookup matches against **canonical** under `COLLATE NOCASE` — functionally equivalent for ASCII, but a deliberate asymmetry worth a comment.

---

## SUMMARY TABLE

| # | Area | Class | Severity |
|---|------|-------|----------|
| 1 | UID = username hash, not sequential; collision → silent overwrite | BUG | HIGH |
| 2 | First account not auto-admin | INTENTIONAL (matches fork) | LOW |
| 3 | passfail_count/bantime failed-login lockout | NOT-IMPLEMENTED | HIGH |
| 4 | kick_old_login (kick old, admit new) | NOT-IMPLEMENTED (reject-only) | MEDIUM |
| 5 | max_accounts limit on create | NOT-IMPLEMENTED | MEDIUM |
| 6 | Case-insensitive dup-reject & login lookup | MATCHES (correct) | — |

### What matches (no action)
- Case-insensitive duplicate-name rejection (Finding 6).
- Case-insensitive login name lookup (Finding 6).
- `passfail_bantime` default = 300 in v3 config (correct vs original; only enforcement missing).
- First-account-not-admin behavior (Finding 2) matches this specific fork.

### Highest-impact items to fix
1. **Finding 1** (UID hash → collision → account-overwrite): real data-loss/security risk.
2. **Finding 3** (no brute-force lockout): security regression vs original.
3. **Findings 4 & 5** (kick_old_login default-behavior inversion; no max_accounts cap): parsed-but-dead config.
