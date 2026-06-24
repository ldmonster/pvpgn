# Bug Hunt — Status

Reference: `/home/cnupt/work/pvpgn-server` (upstream PvPGN-PRO).
Discovery wave 1 complete (10 subsystems). Triage below.

## Discovery coverage
| Subsystem | Findings file | Bugs found |
|---|---|---|
| crypto/hash (NLS/SRP, bnhash) | findings/crypto-hash.md | 1 CRIT + ports verified |
| bnet protocol / wire codec | findings/bnet-codec.md | 0 (codec verified faithful) |
| channel / chat / messages | findings/channel-chat.md | 1 CRIT, 1 HIGH, +5 |
| ladder / rating calc | findings/ladder.md | 3 HIGH, +3 |
| anongame / matchmaking | findings/anongame.md | 2 HIGH, +5 |
| game lists / gameplay | findings/gameplay.md | 1 CRIT, 1 HIGH, +4 |
| clan | findings/clan.md | 2 HIGH, +6 |
| account / attributes | findings/account-attributes.md | 2 CRIT, 2 HIGH |
| D2 realm / character / d2cs | findings/d2-realm.md | 6 HIGH |
| IRC / WOL protocol | findings/irc-wol.md | 1 HIGH, +11 |

## Triage — confirmed by orchestrator (verified against canonical specs)

### TIER 1 — objectively-wrong, fix now (verified)
- **D2-1: `.d2s` class/level offsets + flag masks wrong** (`src/protocol/d2save/src/codec.cpp`).
  extract_class reads off 36 (status) → must be 40; extract_level reads 40 (class) → 43;
  is_hardcore tests 0x01 (INIT) → 0x04; is_expansion tests 0x04 (HARDCORE) → 0x20.
  Verified vs canonical D2S v96 AND original d2charfile.h. Unit test locks in the wrong offsets — fix test too.
- **D2-2: `CharacterClass` enum mis-ordered** (`domain/shared/.../d2_character_class.hpp`,
  `domain/d2cs/types.hpp`). Canonical: amazon0 sorceress1 necromancer2 paladin3 barbarian4 druid5 assassin6.
- **CHAT-1: BnetFsm chat path emits wrong BNCS EID values** (`src/protocol/bnet/src/fsm/fsm_chat.cpp`).
  CHANNEL emitted as 3 (=LEAVE) → 7; INFO as 4 (=WHISPER) → 0x12; JOIN as 1 (=SHOWUSER) → 2.
  Test fsm_channel_test.cpp pins the wrong values — fix test too.
- **IRC-1: `make_numeric` drops the nick argument** (`src/protocol/irc/src/fsm.cpp`) + no PONG handler.

### TIER 2 — confirmed divergence, fix with care
- **LADDER-3: rank sorts by wins, original ranks by rating** (recompute_ladder.cpp) — swapped keys.
- **LADDER-1: initial rating 1500 vs original 1000**.
- **CLAN-1: domain ClanRank enum inverted vs wire, no mapping**.
- **CHAT-2: kick/ban require only membership, not operator** — privilege escalation.

### TIER 3 — real divergence but likely intentional / needs product decision (DOCUMENT)
- **CRYPTO-1: WAR3 login wired to SRP-6a not legacy SRP-3** (faithful SRP-3 port exists, unconnected).
  Big redesign question; do NOT auto-fix.
- **GAME-1: game-type wire-code → GameType mapping wrong** (v3 GameType is a deliberate simplification).
- **ACCT-1..4: attribute key strings differ**; profile-key interop is a real bug, fix those; Record keys document.
- **ANON-1..4: inforeply tag_unk magics, DESC gametype id**.

## Confirmed bug fixes
_(updated as fix fleet lands)_
