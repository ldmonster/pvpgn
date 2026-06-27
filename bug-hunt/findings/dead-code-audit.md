# Dead-code audit (wave 62)

Two fleet agents audited the whole tree (application/domain and protocol/infra/
core/app) for genuinely-unreferenced code, with grep evidence of zero references.
Build context: the project compiles `-Wall -Wextra -Wunused -Werror`, so
file-local dead code already fails the build; this audit targets linkage-level
dead code (whole functions/classes/files nothing calls) and no-op stub blocks.

## REMOVED in wave 62 (unambiguous superseded leftovers, zero footprint)
- `src/protocol/bnet/src/codec_extended.cpp` + `codec_extended.hpp` — a duplicate
  codec that is in NO CMakeLists (never compiled) and included nowhere. The live
  codec is `protocol/bnet/src/codec.cpp`.
- `src/app/bnetd/include/app/bnetd/bnet_session_factory.hpp` (`BnetSessionFactory`)
  — never `#include`d anywhere; production wires the bnet listener via
  `BnetBnftpDispatchFactory`. Superseded leftover.
Verified: removal produced "ninja: no work to do" (zero build footprint) and the
full unit suite stayed 3199/3199.

## NOT removed — needs a human/product decision (catalogued, not purged)
These are genuinely unreferenced but are either intentional scaffolding, an API
surface, or duplicate subsystems where the choice of "which is canonical" is a
judgment call. Removing them unilaterally risks deleting wanted scaffolding.

### Duplicate / superseded subsystems (verify canonical before removing)
- Compiled-but-unreferenced: `protocol/irc/src/bridge_fsm.cpp`
  (`IrcBridgeFsm` — superseded by live `IrcFsm`), `protocol/wolgameres/src/
  wol_fsm.cpp` (stub `wolgameres::WolFsm` — distinct from live `protocol::wol`).
- Uncompiled duplicate dirs: `infra/plugin/` vs live `infra/scripting/plugin/`;
  `infra/scripting/lua/` vs live `infra/lua/`; `infra/sandbox/`. NOTE: the two
  audit agents disagreed on which plugin/lua tree is canonical — must be confirmed
  by reading the composition roots before any removal.
- Built-but-unlinked infra libs: `infra/health`, `infra/session` (4 stub session
  factories), `infra/shadow`, `infra/tracing`, parts of `infra/persistence`
  (adapter_registry, disabled backend_registration), `infra/metrics/server_metrics`,
  `infra/webui/web_server`, `infra/discovery`, `infra/crypto/{peerchat,wol_hash}`.
- Core legacy modules (compiled, zero refs; superseded by header-only replacements):
  `core/util/{peerchat,rcm,proginfo}`, `core/net/addr_list.cpp`, `core/config/conf`,
  `core/error/systemerror`, `core/debug/hexdump` (live = `core/include/core/hexdump.hpp`),
  `core/strings/xstring.cpp` (but KEEP `xstring.h` — `safe_toupper` is used).

### Scaffolding / DTOs awaiting use (intentional — leave unless project says otherwise)
- Snapshot DTOs: `domain/chat/channel_snapshot.hpp`, `domain/gameplay/
  game_snapshot.hpp`, `domain/identity/account_snapshot.hpp`, `domain/social/
  clan_snapshot.hpp` (persistence currently uses `rehydrate()` directly).
- Interface headers with no impl/use: `application/ads/ads_repository.hpp`,
  `application/realm/d2_ladder_repository.hpp`.
- 18 per-context TODO-stub headers: `domain/{chat,connection,gameplay,identity,
  ladder,matchmaking,moderation,realm,social}/{events,errors}.hpp` — each just a
  `// TODO: Define ... here` placeholder (the LIVE events file is
  `domain/shared/events.hpp` — do NOT touch that). Keeping them preserves the
  per-context module convention.
- Individual dead symbols (zero refs): `IConnectionHandler`,
  `shared::WhisperDelivered` (struct + variant arm), d2cs `GameInfo`, d2dbs
  `GameResultData`, `Realm::characters()`, chat `Whisper`/`WhisperOutcome`,
  `IgnoreList::ignored_list()`, `ChatMessage::size()`, `IWolGameStore::remove`,
  plus ~40 small dead functions in core (`bn_type.cpp`, `trans.cpp`, tag helpers)
  and several infra `repository_factory` create_* methods.

### UNWIRED-BUT-TESTED application/domain API (definitely keep)
A large, deliberate surface: `application/{admin_commands,i18n,icon_table,init,
email_management,anongame_lobby,ladder,profile,tournament,anongame_inforeply}`,
chat use-cases `KickFromChannel`/`BanFromChannel`/`OpFromChannel`/`SendEmote`/
`SendWhisper`/`SetChannelTopic`, auth `LockAccount`/`UnlockAccount`/`ListSessions`/
`PasswordUpgrade`, moderation `BanIp`/`BanAccount`/`KickConnection`/etc.,
social clan/team use-cases, game `CreatePrivateGame`/`CancelGame`/
`ReportGameResult`, realm use-cases. These have unit tests and are intentional
application-layer API not yet wired into the bnetd composition root.

## Dead stub / no-op blocks (fix or leave; not removed)
- `fsm_auth.cpp` ~line 111: `if (use_cases_.check_ip_ban) { /* TODO commented
  execute() */ }` — no-op block; the CheckIpBan use-case is tested but its call is
  commented out (IP-ban enforcement is unimplemented in the FSM).
- `application/chat/src/op_from_channel.cpp` ~44: discards the grant/revoke intent
  (`(void)cmd.grant;`) — OpFromChannel has no effect.
- `application/realm/src/character_persistence.cpp` ~152: `list_characters()` is a
  TODO no-op returning an empty vector.

## Recommendation
The codebase is largely clean (`-Werror`, heavily tested); the dominant pattern is
intentional unwired-but-tested API and per-context scaffolding, NOT rot. The
clearly-removable cruft is the duplicate/superseded subsystems and core legacy
modules — but several involve a "which is canonical" decision (esp. plugin/lua)
that should be confirmed against the composition roots, ideally in a dedicated
cleanup pass with the build + full test suite run after each batch.

## UPDATE waves 63-64: removed orphaned/dead subsystems
- w63: src/infra/plugin/ (orphan dup of infra/scripting/plugin) + src/infra/sandbox/
  + tests/unit/infra/sandbox/ (orphan lib+test, no CMakeLists, never built).
- w64: protocol/bnet event_dispatcher.{cpp,hpp} (BnetEventDispatcher) — was compiled
  (CMakeLists:574) but referenced by nothing; removed file + CMake line, full relink
  + suite confirm dead.
Still NOT removed (need care): infra/scripting/lua (unwired-but-TESTED, Lua-gated);
the infra/session stub factories + protocol/irc bridge_fsm + protocol/wolgameres
cluster (interdependent — bridge_fsm/wolgameres referenced only by the dead session
factories); core legacy modules. Confirm canonical + build BOTH Lua-on/off before
touching the scripting items.

## UPDATE wave 65: removed core/util peerchat + proginfo
Removed src/core/util/{peerchat,proginfo}.{cpp,h} (zero #include anywhere; the
only "peerchat" include hits a DISTINCT infra/crypto/peerchat). core_util retains
util_file/token/trans/rcm/list. Build relinked + 3199/3199.
NOTE corrections from precise verification (substring grep gave false positives):
- core/util/rcm is USED (bniutils tools + trans/util_string/conf) — KEEP.
- core/error/systemerror: removing it would empty the core_error STATIC lib (its
  only source) — needs core_error dependency analysis first; LEFT for now.
- core/debug/hexdump + tests/unit/core/hexdump_test.cpp reference each other —
  verify the test targets the legacy .cpp vs the header-only core/hexdump.hpp
  before removing.
- core/net/addr_list referenced by addr_core.cpp — verify symbol use before removing.
Dead-code removal paused here: remaining items need per-symbol / lib-dependency
care (error-prone via grep) and are best done in a focused pass with build+test.

## UPDATE wave 75: removed infra/health (unlinked HTTP-handler lib)
Removed the infra_health STATIC lib (src/CMakeLists.txt block) + src/infra/health/.
Proven dead: no target links infra_health, no `#include "infra/health/*"` anywhere,
HealthHandler/MetricsHandler referenced nowhere outside their own dir, no test dir.
Reconfigure + full relink + 3199/3199; diff_chat still matches the oracle (bnetd
never linked it, so the binary is byte-for-byte unchanged in behavior).
Still present (have real consumers/tests, do NOT remove): infra/discovery (used by
services/combined + test), infra/metrics + infra/webui_json (tested), infra/tracing
(unlinked but is a deliberate option-gated OTLP adapter — leave as scaffolding).

## UPDATE wave 66: removed dead infra/session + IrcBridgeFsm cluster
Removed the infra_session INTERFACE library (src/CMakeLists.txt block) and its 4
protocol session-factory headers (src/infra/session/), confirmed linked by nothing
(production wires session factories from src/app/bnetd). Its only consumer-side
reference to protocol/irc IrcBridgeFsm made bridge_fsm.{cpp,hpp} dead too (a
superseded duplicate of the live IrcFsm) — removed file + protocol_irc compile
line. wolgameres was EXCLUDED (it has live refs: bnet_packet_pump/conn_class.hpp
+ a test). Fixed 2 stale doc-comments that pointed at the deleted infra/session.
Reconfigure + full relink (incl. protocol_irc tests + bnetd) + suite: 3199/3199.

## UPDATE wave 83: removed infra/shadow (unlinked shadow-write migration lib)
Removed the pvpgn_infra_shadow STATIC lib (src/CMakeLists.txt block 1473-1475) +
src/infra/shadow/ (ShadowAccountRepository / ShadowUnitOfWork[Factory] — a
dual-backend shadow-write adapter for zero-downtime migration). Proven dead: no
target links pvpgn_infra_shadow, no `#include "infra/shadow/*"` anywhere, the
classes referenced nowhere outside their dir, no test dir, persistence
backend_registration/adapter_registry don't reference it. Reconfigure + full
relink + 3199/3199; bnetd never linked it (binary behaviorally unchanged,
diff_chat still matches the oracle).
