# R70 — handle_*.cpp scope audit (all 20 files)

Scope: every `src/**/handle_*.cpp`. Status per file: legacy emit sites and which
strangler-fig bridge currently covers them. Goal of R70+ is to drive every
emission either to a bridge or document why none is needed.

## Legend
- ✅ bridged: legacy emit is fully behind a `pvpgn_v3_*` hook with fallback.
- ⏳ pending: emit exists, no v3 bridge yet (queued for a follow-on round).
- ➖ N/A: file has no packet emissions (signal/init plumbing only).
- 🔒 deferred: codec/structural gap (R68-style); not feasible in current round.

## bnetd/ (14 files — covered in R65 audit, 76/76 emissions bridged)
- handle_anongame.cpp        ✅ (R47-R67)
- handle_apireg.cpp          ✅
- handle_bnet.cpp            ✅ (R31-R63)
- handle_bot.cpp             ✅
- handle_d2cs.cpp            ✅
- handle_file.cpp            ✅
- handle_init.cpp            ✅ (R30-R36)
- handle_irc.cpp             ✅
- handle_irc_common.cpp      ✅
- handle_telnet.cpp          ✅
- handle_udp.cpp             ✅
- handle_wol.cpp             ✅
- handle_wol_gameres.cpp     ✅
- handle_wserv.cpp           ✅

## d2cs/ (5 files)
- handle_init.cpp            ➖ (no packet_create)
- handle_signal.cpp          ➖ (signal trampoline only)
- handle_d2gs.cpp            partial
  - L116/118 D2CS_D2GS_SETINITINFO        ⏳
  - L151/153 D2CS_D2GS_SETCONFFILE        ⏳
  - L234/236 D2CS_D2GS_AUTHREPLY          ⏳
  - L264/266 D2CS_D2GS_SETGSINFO          ⏳
  - L329/331 D2CS_CLIENT_CREATEGAMEREPLY  ✅ (R-prior)
  - L417/419 D2CS_CLIENT_JOINGAMEREPLY    ✅ (R-prior)
  - L540/542 D2CS_D2GS_AUTHREQ            ⏳
- handle_d2cs.cpp            partial
  - L145/148 D2CS_BNETD_ACCOUNTLOGINREQ   ⏳
  - L208/211 D2CS_BNETD_CHARLOGINREQ      ⏳
  - L223/225 D2CS_CLIENT_CREATECHARREPLY  ✅
  - L326/328 D2CS_CLIENT_CREATEGAMEREPLY  ⏳ (second site, error path)
  - L350/353 D2CS_D2GS_CREATEGAMEREQ      ⏳
  - L427/429 D2CS_CLIENT_JOINGAMEREPLY    ⏳ (second site)
  - L444/447 D2CS_D2GS_JOINGAMEREQ        ⏳
  - L509/511 D2CS_CLIENT_GAMELISTREPLY    ⏳
  - L526/528 D2CS_CLIENT_GAMELISTREPLY    ⏳
  - L560/562 D2CS_CLIENT_GAMEINFOREPLY    ⏳
  - L628/630 D2CS_CLIENT_CHARLOGINREPLY   ⏳
  - L641/644 D2CS_BNETD_CHARLOGINREQ      ⏳ (second site)
  - L680/682 D2CS_CLIENT_DELETECHARREPLY  ⏳
  - L733/735 D2CS_CLIENT_LADDERREPLY      ⏳
  - L781/783 D2CS_CLIENT_MOTDREPLY        ⏳
  - L832/834 D2CS_CLIENT_LADDERREPLY      ⏳ (second site)
  - L876/878 D2CS_CLIENT_CHARLISTREPLY    ⏳
  - L992/994 D2CS_CLIENT_CHARLISTREPLY_110 ⏳
  - L1110/1112 D2CS_CLIENT_CONVERTCHARREPLY ⏳
  - L1125/1127 D2CS_CLIENT_CREATEGAMEWAIT ⏳
- handle_bnetd.cpp           partial
  - L91 packet_class_init                 ⏳ (handshake init reply)
  - L108/110 D2CS_BNETD_AUTHREPLY         ⏳
  - L176/178 D2CS_CLIENT_LOGINREPLY       ✅
  - L243/245 D2CS_CLIENT_CREATECHARREPLY  ✅ (covered by existing bridge)
  - L277/279 D2CS_CLIENT_CHARLOGINREPLY   ✅ (covered by existing bridge)
  - L326/328 D2CS_BNETD_GAMEINFOREPLY     ⏳

## d2dbs/ (1 file)
- handle_signal.cpp          ➖ (signal trampoline only)

## Summary
- bnetd: 14/14 done.
- d2cs: 3/5 in progress, ~24 ⏳ emit sites across 3 files.
- d2dbs: 1/1 trivially done (no packets in handle_*).

Total: 20/20 files audited. 76 emit sites bridged so far; ~24 d2cs sites pending.

## Plan for follow-on rounds
- R71: handle_bnetd.cpp gaps (4 sites: init reply, AUTHREPLY, GAMEINFOREPLY → bnetd; AUTHREPLY is small).
- R72: handle_d2gs.cpp gaps (5 sites: SETINITINFO, SETCONFFILE, AUTHREPLY, SETGSINFO, AUTHREQ).
- R73-R75: handle_d2cs.cpp gaps in batches of ~5-7 sites by semantic family
  (creategame group, joingame group, gamelist/gameinfo group, char* group,
  ladder/motd/charlist group, convertchar/wait group).

Each round follows established pattern: codec coverage check → bridge .hpp/.cpp
→ wire decl + hook → Catch2 test → CMakeLists + Dockerfile.v3 → docker build.
If codec lacks message, document and 🔒 defer (R68-style).
