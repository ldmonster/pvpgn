# Differential protocol testing — original pvpgn-server vs v3

Protocol-faithful Battle.net (BNCS) mock clients that drive **both** the upstream
`pvpgn-server` (the known-good oracle) and this v3 rewrite, so their wire
behaviour can be diffed. This is how the auth/login fixes were validated against
ground truth rather than against v3's own assumptions.

## Pieces
- `bncs_client.py` — a BNCS client library: packet framing, the legacy
  broken-SHA-1 (`blizzard_hash`), OLS double-hash login, account creation, and an
  adaptive AUTH_INFO handshake that copes with both servers' sequences.
- `original_server.py` — launches the upstream `bnetd` in an isolated temp home
  (conf copied from its build tree, paths rewritten, support-file placeholders,
  a test port).
- `v3_server.py` — launches the v3 `bnetd` (inmemory backend).
- `diff_ols_login.py` — runs the OLS create+login scenario against both and
  prints a field-by-field comparison, asserting login *outcomes* match the oracle.

## Prerequisites
Build the original server once (file-storage backend, ZLIB only):
```
cd /home/cnupt/work/pvpgn-server
cmake -S . -B build -DWITH_BNETD=ON -DWITH_D2CS=OFF -DWITH_D2DBS=OFF \
      -DWITH_WIN32_GUI=OFF -DWITH_LUA=OFF
cmake --build build -j
```
Build v3 `bnetd` as usual (`build/v3-dev/src/app/bnetd/bnetd`).

## Run
```
python3 tests/diff/diff_ols_login.py --v3-bnetd build/v3-dev/src/app/bnetd/bnetd
```

## What it has found / confirmed
- **CRITICAL (fixed):** v3 ignored the `0x01` CLIENT_INITCONN_CLASS_BNET octet
  that every real client sends first (it only recognised a bare `0xFF`). Real
  clients hung at connect. Fixed in `bnet_bnftp_dispatch.cpp`; regression added
  as the e2e "real-client init byte" journey.
- **Validated:** v3's OLS login is now *outcome-equivalent* to the oracle —
  accept (0x00), wrong password (0x02), unknown account (0x01) all match, using
  the real broken-SHA-1 double-hash with the server token.
- **Documented divergence (informational):** v3's AUTH_INFO reply is a bare
  result, not the SERVER_AUTHREQ seed (no `logon_type`/`server_token`/MPQ
  formula) and v3 skips the client AUTH_CHECK step. Real clients tolerate the
  simplified flow for login, but the seed/AUTH_CHECK gap is the next auth item
  (see bug-hunt/findings/cdkey-authcheck.md + nls-auth-flow.md).

## Note
These tests are not part of `check-all` (they need the upstream server built).
The e2e gate covers the real-client init-byte path without the oracle.
