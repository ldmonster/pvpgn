# Bug Hunt — user/admin Commands subsystem

ORIGINAL: `/home/cnupt/work/pvpgn-server/src/bnetd/command.cpp` (5241 lines) + `command_groups.cpp` + `conf/command_groups.conf.in`
CURRENT v3: `src/application/admin_commands/src/router.cpp`, `src/application/chat/src/*`, `src/application/auth/src/permission_checker.cpp`, `src/protocol/bnet/src/fsm/fsm_chat.cpp`

## Architecture note (how the two models differ)

The **original** permission model is *purely data-driven*. At dispatch (`handle_command`,
command.cpp:590-617) every command in `standard_command_table` is gated by
`command_get_group(cmd) & account_get_command_groups(account)`. The command→group(bitmask
of groups 1-8) mapping comes entirely from `command_groups.conf`. Two consequences:
- A command with **no** config entry is reported "This command has been deactivated".
- For commands like `/kick /ban /op`, the command-group gate is only a *coarse* first gate
  (default config puts them in group **1** = every user). The *real* permission is the
  per-channel operator/admin/tempOP check **inside** each handler.

The **v3** model replaces this with a hard-coded enum (`domain::moderation::Permission`) and a
hard-coded group→permission table (`InMemoryPermissionChecker::init_group_mappings`). Group
numbers are hard-wired (1=admin, 2=mod, 3=operator, 4=voice), groups 5-8 are ignored, and the
per-channel op/admin/tempOP distinction is collapsed. This is the root cause of several findings
below.

---

## FINDINGS

### F1 — Command-group model is hard-coded, not config-driven (groups 5-8 dropped)
- Severity: HIGH | Classification: BUG
- Original `command_groups.cpp:42-102` loads an arbitrary command→group bitmask from
  `command_groups.conf`; `command.cpp:594-603` honors all 8 groups and emits
  "This command has been deactivated" for unmapped commands.
- v3 `src/application/auth/src/permission_checker.cpp:72-88` only inspects groups 1-4 and maps
  them to fixed names ("admin"/"mod"/"operator"/"voice"). Groups 5-8 are silently ignored:
  ```cpp
  for (std::uint8_t group_num = 1; group_num <= 8; ++group_num) {
      if (groups.has(group_num)) {
          if (group_num == 1) group_name = "admin";
          else if (group_num == 2) group_name = "mod";
          else if (group_num == 3) group_name = "operator";
          else if (group_num == 4) group_name = "voice";   // 5-8 → group_name stays ""
  ```
- Divergence: In the stock config, `/operator /admin /flag /tag` are group **6** and
  `/admin-con /admin-addr /admin-chpass` group **6**; `/serverban /ipban` group **3**. An
  account whose only elevated group is 5-8 gets *no* permissions in v3 even though the original
  would grant them. Also the original's per-command configurability is lost entirely.
- Fix: Drive permissions from the loaded command_groups data (command string → group bitmask),
  intersect against `account.command_groups()`, for all 8 groups. Don't hard-code group→name.

### F2 — `/kick` does not protect admins/operators and ignores channel-op auth
- Severity: HIGH | Classification: BUG
- Original `command.cpp:2505-2536`: kicker must be channel/global admin OR operator OR tempOP;
  and it is forbidden to kick admins ("You cannot kick administrators.") or operators
  ("You cannot kick operators.").
- v3 `src/application/chat/src/kick_from_channel.cpp:10-44`: only checks self-kick, channel
  existence, membership of kicker+target, then `channel.kick()`. No admin/operator-immunity
  check, no per-channel op/tempOP authorization — relies solely on `channel.kick()` returning
  false.
- Divergence: A privileged caller can kick admins/operators (original explicitly blocks this);
  also the authorization model (op/admin/tempOP) is replaced by bare membership + an opaque
  domain method. Reply texts also differ entirely.
- Fix: Re-introduce the admin/operator immunity checks and per-channel op/tempOP authorization;
  match the original error strings.

### F3 — `/ban` does not protect admins/operators
- Severity: HIGH | Classification: BUG
- Original `command.cpp:2583-2610`: requires channel/global admin or operator; blocks banning
  admins ("You cannot ban administrators.") and operators ("You cannot ban operators.").
- v3 `src/application/chat/src/ban_from_channel.cpp:24-39`: only checks self-ban, channel
  existence, and `channel.contains(banner)`; then `channel.kick()`. No admin/operator immunity.
- Divergence: same class as F2 — privileged users can ban admins/operators. Note also v3 returns
  `TargetAlreadyBanned` (no original analogue; original just re-runs `channel_ban_user`).
- Fix: add the immunity checks and proper channel-op authorization; reconcile error semantics.

### F4 — `/op` uses a global "operator" group instead of per-channel op/admin/tempOP, and is a no-op
- Severity: HIGH | Classification: BUG
- Original `command.cpp:1238-1290`: OP_lvl=1 if `account_is_operator_or_admin(acc, channel)`,
  OP_lvl=2 if `channel_conn_is_tmpOP`; full ops promote to durable Channel Operator
  (`account_set_auth_operator`), tempOPs may only tempOP and only same-channel targets.
- v3 `src/application/chat/src/op_from_channel.cpp:13-58`: permission is
  `permissions_->has_command_group(requester, "operator")` (a *global* command group, not
  per-channel auth, and no tempOP path). Worse, granting op is a structural **no-op**:
  ```cpp
  // The Channel aggregate does not yet expose a dedicated grant_op ...
  (void)cmd.grant;            // nothing is actually granted/revoked
  ```
- Divergence: wrong authorization basis (global group vs channel op/admin/tempOP) AND the op
  grant has no effect — the command silently does nothing.
- Fix: gate on per-channel operator/admin/tempOP; actually persist the op grant (add
  `Channel::set_operator`).

### F5 — `/whisper` blocks self-whisper and mutes the mute-check; AWAY semantics differ
- Severity: MED | Classification: BUG
- Original `do_whisper` (`command.cpp:169-219`): order is
  (1) muted account → refuse ("Your account has been muted…"),
  (2) target offline → "That user is not logged on.",
  (3) target DND → blocked with "is unavailable",
  (4) deliver; if target AWAY, **still delivered** plus an "is away" notice to sender.
  There is **no self-whisper restriction** — whispering yourself is allowed.
- v3 `decide_whisper` (`src/application/chat/src/whisper_use_case.cpp:32-40`):
  ```cpp
  if (req.target.empty())          return NoTarget;
  if (is_blank(req.body))          return EmptyBody;
  if (ieq(req.sender, req.target)) return SelfWhisper;   // original ALLOWS self-whisper
  if (!req.target_state.online)    return TargetOffline;
  if (req.target_state.dnd)        return TargetDnd;
  if (req.target_state.ignored_by) return IgnoredByTarget;
  return Delivered;
  ```
- Divergences: (a) v3 rejects self-whisper, original allows it. (b) v3 has **no mute check**
  (a muted user can whisper in v3). (c) AWAY is absent from v3's verdict — original delivers the
  message *and* notifies; v3 cannot reproduce the "delivered-with-away-notice" outcome.
- Fix: drop the SelfWhisper block (or confirm intentional), add a muted-sender verdict checked
  first, and model AWAY as "Delivered + sender notice" rather than a block.

### F6 — `/away` and `/dnd` have no bnet handler (routed but unimplemented), default strings missing
- Severity: MED | Classification: BUG (parity gap on an implemented-looking command)
- Original `command.cpp:2338-2390`: `/away` toggles; with no arg sets default away string
  `"Currently not available"`; with arg sets that string. `/dnd` toggles; default
  `"Not available"`. Both manage `conn_get/set_awaystr` / `dndstr`.
- v3: `/away` and `/dnd` appear in the alias table (`router.cpp:45-46`) so they *route*, but
  there is no away/dnd state nor handler in the bnet path. The only AWAY logic anywhere is IRC
  (`src/protocol/irc/src/fsm/irc_commands.cpp:305-317`), a different protocol with different
  reply text ("You have been marked as being away" vs bnet "You are now marked as being away").
- Divergence: a routed command does nothing on bnet; no away/dnd state for whisper to consult
  (compounds F5). Default strings ("Currently not available" / "Not available") are absent.
- Fix: implement bnet away/dnd toggle + state, with the original default strings, and feed it
  into whisper delivery.

### F7 — Dispatch path doesn't reproduce original `/cmd` parsing (whisper-target split lost)
- Severity: MED | Classification: BUG
- Original routes `/w /whisper /msg /m` to `_handle_whisper_command`, which splits "target rest"
  and delivers via `do_whisper`.
- v3 bnet FSM `fsm_chat.cpp:203-244`: for any `/`-prefixed text it strips the `/` and calls
  `command_registry->dispatch(account, rest, checker)` (`command_registry.cpp:15-43`), which
  splits into name + whitespace args and looks up `commands_`. The richer
  `classify_chat_command` (`chat_command.cpp:40-82`, which knows how to split a whisper target
  from body for `/w /whisper /msg /m`) is **not used on this path** — it's dead on the bnet FSM.
  Whether `/whisper` works depends entirely on a registry entry that is registered nowhere in
  `src/` (no `register_command` call outside the registry's own TU and the Lua registry).
- Divergence: as wired, bnet `/whisper` (and every other `/cmd`) resolves to "Unknown command"
  unless something registers it; the whisper-target parsing that exists is unreachable.
- Fix: wire the chat use-cases into the registry (or call `classify_chat_command` in the FSM);
  add coverage that bnet `/w user msg` actually delivers.

### F8 — Unknown-command and permission-denied reply texts differ
- Severity: LOW | Classification: BUG (cosmetic/parity)
- Original (`command.cpp:601`, `:625`): "This command is reserved for admins." /
  "Unknown command." / "This command has been deactivated".
- v3 `fsm_chat.cpp:222-229`: "Unknown command. Type /help for a list of commands." /
  "You do not have permission to use that command." (and there is no "deactivated" state).
- Fix: match original strings (and add the deactivated case from F1) if byte-parity matters.

### F9 — `/announce` group requirement not reproduced
- Severity: MED | Classification: UNSURE (depends on registry wiring, which is absent)
- Original: `/ann /announce` are group **2** in stock config (`command_groups.conf.in:152`);
  handler `command.cpp:1973` formats "Announcement from {}: {}" and `message_send_all`.
- v3: `/announce` routes (`router.cpp:80-81`) but `route()` only consults the
  `is_permitted` predicate the caller passes; there is no built-in announce handler or fixed
  group requirement in v3, and no `Permission::Announce` exists in the permission enum. Whether
  the group-2 gate is honored is entirely caller-dependent and currently unimplemented.
- Fix: ensure announce is gated by the same configurable group and produces the same text.

---

## v3 command COVERAGE (commands v3 has machinery for)

| Command | v3 location | Status |
|---|---|---|
| `/join` `/channel` `/j` | fsm_chat.cpp on(JoinChannel) + join_channel.cpp | PARTIAL — join works; error strings are v3-invented |
| channel message (no slash) | fsm_chat.cpp on(ChatCommand) + post_message.cpp | OK-ish |
| `/whisper` `/w` `/m` `/msg` | whisper_use_case.cpp (logic) + router alias | DIVERGES (F5), wiring broken (F7) |
| `/kick` | kick_from_channel.cpp | DIVERGES (F2) |
| `/ban` | ban_from_channel.cpp | DIVERGES (F3) |
| `/op` `/deop` (?) | op_from_channel.cpp | DIVERGES (F4) — and no-op |
| `/topic` | set_channel_topic.cpp | present (not deeply audited here) |
| `/me` `/emote` | send_emote.cpp + router alias | present (not deeply audited) |
| `/channels` `/chs` | list_channels.cpp + fsm ChannelListRequest | present |
| `/away` `/dnd` | router alias only | NOT IMPLEMENTED on bnet (F6) |
| `/announce` `/ann` | router alias only | NOT IMPLEMENTED / ungated (F9) |
| `/help` `/?` | admin_commands help_* + file_help_responder | present (help corpus) |
| `/version /uptime /who /whoami /finger /time /news /games /motd /copyright /lusers /connections /admins /quit /beep /nobeep /squelch ...` | router alias → "bodies still in legacy command.cpp" per router.cpp comments | BRIDGE ONLY — v3 routes but defers to legacy handlers; not reimplemented |

The router (`router.cpp`) is explicitly a **legacy bridge**: most aliases map to canonicals whose
bodies "are still in legacy command.cpp" (comments at router.cpp:24, :142). So for the large
read-only/info command set, v3 intends to delegate, not reimplement — those are not bugs, just
not-yet-ported. The genuinely *reimplemented* commands (kick/ban/op/whisper/join/topic) are where
the parity bugs above live.

## Commands the original has that v3 does not implement (scope gaps, not bugs)
`/unban` (handler 2635, no v3 use-case), `/aop /vop /voice /devoice /tmpop /deop` (only a partial
`/op`), `/watch /unwatch /watchall /unwatchall`, `/finger`, `/squelch /unsquelch` (state),
`/flag /tag`, `/mail`, `/friends` (`/f`), `/clan` (`/c`), `/addacct /chpass /lockacct /muteacct
/kill /killsession /rehash /shutdown /serverban /ipban /find /set /save /config /quota /netinfo
/ipscan /timer /ping /commandgroups`, `/rejoin`, `/reply` (`/r`), `/realmann`, `/moderate`,
`/gameinfo /ladderactivate /ladderinfo /stats /lusers /connections`. These route through the
legacy bridge but have no v3-native reimplementation.

## Summary
9 findings. Root cause for several is the permission-model rewrite: v3 hard-codes group→name
(F1, drops groups 5-8) and replaces per-channel op/admin/tempOP auth with global command-groups,
losing both configurability and the admin/operator immunity that protected privileged users from
being kicked/banned (F2/F3/F4). Whisper diverges on self-whisper, mute, and away (F5); away/dnd
have no bnet handler at all (F6); and the bnet `/cmd` dispatch path doesn't use the
whisper-aware parser and registers no concrete commands, so reimplemented commands aren't
actually reachable as wired (F7). Reply-text parity is broken in several spots (F8). Most legacy
info/read-only commands are intentionally bridged to legacy code, not reimplemented (not bugs).

---

## F-W15 — /whisper (/w /msg /m) not implemented; private messages dropped
**Severity:** HIGH (a core chat feature was entirely missing)
**Classification:** NOT-IMPLEMENTED — fixed (wave 15)

**Symptom (differential, tests/diff/diff_whisper.py):** Alice sends
`/w bob <msg>`. The original delivers `EID_WHISPER` (0x04, username=alice) to
Bob and `EID_WHISPERSENT` (0x0a, username=bob) back to Alice. v3 routed `/w`
through the generic CommandRegistry, which didn't know the command, so Alice got
`EID_INFO "Unknown command…"` and Bob received nothing.

**Original ref:** src/bnetd/command.cpp `do_whisper` + the `/msg /whisper /w /m`
alias table; src/bnetd/message.cpp maps message_type_whisper→EID_WHISPER(0x04),
message_type_whisperack→EID_WHISPERSENT(0x0a), message_type_error→EID_ERROR(0x13).

**Fix:** BnetFsm::on(ChatCommand) now intercepts the whisper alias family
*before* the generic dispatch (whisper isn't a text-returning command — it routes
to another online session). New BnetFsm::handle_whisper parses `<target> <msg>`,
resolves the target via account_repo->find_by_name + session_registry->session_for,
routes EID_WHISPER to the target's session through the message router, and sends
EID_WHISPERSENT back to the sender. Offline/unknown target → EID_ERROR
"That user is not logged on." (matches the original's message_type_error).

**Verified:** diff_whisper.py — both clients now match the oracle byte-for-byte
(`bob:[(4,'alice','hey there')] alice:[(10,'bob','hey there')]`). Unit guards in
tests/unit/protocol/bnet/fsm_channel_broadcast_test.cpp: routing of EID_WHISPER +
EID_WHISPERSENT, all four aliases, and the offline-target EID_ERROR path.

---

## F-W18 — /me (/emote) not implemented; emotes dropped
**Severity:** MEDIUM (a standard chat command was missing)
**Classification:** NOT-IMPLEMENTED — fixed (wave 18)

**Symptom (differential, tests/diff/diff_emote.py):** Alice sends `/me waves`.
The original broadcasts EID_EMOTE (0x17, username=alice) to the whole channel,
INCLUDING the sender. v3 routed `/me` through the generic CommandRegistry, which
rejected it as unknown, so the emote was dropped entirely.

**Original ref:** src/bnetd/command.cpp `_handle_me_command` ("/me","/emote") ->
`channel_message_send(channel, message_type_emote, …)`; channel.cpp:734 shows the
server skips the speaker ONLY for message_type_talk — TALK is suppressed for the
sender, but EMOTE/WHISPER are echoed back to the sender too.

**Fix:** BnetFsm::on(ChatCommand) intercepts /me and /emote before the generic
dispatch; new BnetFsm::handle_emote reuses PostMessage to validate channel
membership + resolve the other-member recipient set, broadcasts EID_EMOTE to
them, AND echoes EID_EMOTE back to the sender via ctx_ (the TALK/EMOTE self-echo
asymmetry). Not-in-a-channel -> EID_ERROR "You are not in a channel."

**Verified:** diff_emote.py matches the oracle (bob AND alice each get
(0x17,'alice','waves')). Confirmed separately that the original does NOT echo
TALK to self — v3's TALK already excludes the sender, so it stays correct. Unit
guards in fsm_channel_broadcast_test.cpp: emote-to-others+self-echo, /emote alias,
and the not-in-channel EID_ERROR path.

---

## F-W18b — EID_JOIN omits the joiner's statstring
**Severity:** LOW — found wave 18, FIX PENDING
**Classification:** PARTIAL

When Bob joins Alice's channel the original sends Alice EID_JOIN (0x02) with the
joiner's statstring as the event text (e.g. "BOON" — product + record), plus a
couple of EID_USERFLAGS updates. v3 sends EID_JOIN with an EMPTY text and no
USERFLAGS follow-ups. The join IS announced (event id + username correct), so
rosters populate; only the statstring/flags payload is missing. Deferred: needs
the client statstring plumbed from ENTERCHAT/login through the join broadcast.

## Wave 57: /whoami implemented; /whois offline-wording note
/whoami was unimplemented (fell through to the command registry -> "Unknown
command."). Added BnetFsm::handle_whoami() reporting the caller's own location
("You are using Battle.net and are currently in channel ..."), mirroring the
original's _handle_whoami_command (do_whois on self). Verified by
tests/diff/diff_whoami.py (reply kind EID_INFO, not unknown-command; the
localized text itself is charset-garbled in the harness so only the kind is
compared). Minor known gap (not fixed): /whois on an existing-but-offline,
bnet-class user — the oracle returns "User was last seen on: <timestamp>" while
v3 returns the flat "User is offline" (oracle's non-bnet fallback wording, which
v3 matches). Needs last-seen tracking; the timestamp wouldn't diff cleanly anyway.
