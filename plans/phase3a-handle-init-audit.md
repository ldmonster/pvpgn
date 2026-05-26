# Phase 3.A kickoff (R167) — handle_init audit

> First-pass audit of `src/bnetd/handle_init.cpp`, the smallest of the
> Phase 3 targets per `plans/phase3-plan.md`. Goal: identify what is
> needed to delete the file outright.

## Current state

- File size: 230 lines.
- Single entry: `handle_init_packet(t_connection*, t_packet*)`.
- Handles exactly one packet type: `CLIENT_INITCONN` (6 cclass
  variants: BNET, FILE, BOT, TELNET, D2CS_BNETD, ENC, LOCALMACHINE).
- Strangler bridge already present under `PVPGN_V3_BNETD_INTEGRATION`:
  - `pvpgn_v3_init_conn_decide(cclass, &v3_decision)` -- decides
    accept/reject. Output compared to legacy table; mismatch logs a
    warning. Live in production today.
  - `pvpgn_v3_init_conn_apply(c, cclass)` -- if installed (via
    `install_init_conn_apply_handler`), transitions the connection to
    the right `conn_class_*`. Returns 1 (handled) or -1 (handled but
    rejected), and the legacy switch is skipped.

## What blocks deletion of handle_init.cpp

1. `pvpgn_v3_init_conn_apply` requires
   `install_init_conn_apply_handler()` to have been called at startup.
   Need to confirm this is now wired unconditionally for both legacy
   `bnetd` and the v3 `pvpgn_v3_bnetd` binaries (or just for v3 if
   legacy is being retired per L-plan).
2. The `CLIENT_INITCONN_CLASS_D2CS_BNETD` branch calls
   `handle_d2cs_init(c)` for realmlist gating. Need to confirm the v3
   apply handler reproduces this (an audit of `application::init`).
3. Connection-count rate-limit (`prefs_v3::max_conns_per_IP()` +
   `connlist_count_connections()`) currently lives in the legacy
   prologue, BEFORE the v3 decide hook. Either move into the v3
   handler or keep as a pre-bridge guard.
4. Packet-class sanity check (`packet_get_class != packet_class_init`)
   ditto -- this is normally caught one layer up; legacy keeps it as
   defense in depth.

## Suggested R168 / R169 work units

| Round  | Work                                                         |
| ------ | ------------------------------------------------------------ |
| R168.a | Ensure `install_init_conn_apply_handler()` is mandatory at v3 startup; fail-fast if NULL. |
| R168.b | Move max_conns_per_IP rate-limit into the v3 decide path; verify with a new Catch2 case. |
| R168.c | Audit `dispatch_init_conn` vs `handle_d2cs_init` realmlist gate; add a domain-level realmlist check if missing. |
| R169.a | Under `#ifdef PVPGN_V3_BNETD_INTEGRATION`, replace the legacy switch with a `return 0;` -- v3 owns init exclusively. Keep parity-warn for one release. |
| R169.b | Delete `handle_init.cpp` (+ remove from `src/bnetd/CMakeLists.txt`). The `pvpgn_v3_init_*` bridge stays. |

## Acceptance gate

- Docker v3-test green.
- Manual smoke: bnetd accepts a Diablo II + StarCraft + WAR3 + telnet
  client without rejection on the cclass byte.
- Eventlog shows zero "v3/legacy init dispatch mismatch" warnings.

## Out of scope

`handle_auth.cpp` does NOT exist in this tree -- auth lives in
`handle_bnet.cpp::handle_bnet_login_*`, which is ~10x larger and
will be its own Phase 3.B work item. Phase 3.A is `handle_init` only.
