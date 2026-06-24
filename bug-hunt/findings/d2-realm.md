# Bug Hunt — Diablo II realm / character / d2cs subsystem

Comparison of ORIGINAL PvPGN (`/home/cnupt/work/pvpgn-server`) vs v3 rewrite
(`/home/cnupt/work/pvpgn`). Focus: `.d2s` save parsing, character class/flags,
realm char-list, d2cs<->d2dbs packets.

Reference for the real D2 v1.09/v1.10 `.d2s` fixed-header offsets (confirmed by
the ORIGINAL constants in `src/d2cs/d2charfile.h`):

| field          | offset (1.09+) | original constant                          |
|----------------|----------------|--------------------------------------------|
| signature      | 0x00 (0)       | (0xAA55AA55)                               |
| version        | 0x04 (4)       | `D2CHARSAVE_VERSION_OFFSET`                 |
| file size      | 0x08 (8)       | —                                          |
| checksum       | 0x0C (12)      | `D2CHARSAVE_CHECKSUM_OFFSET`               |
| active weapon  | 0x10 (16)      | —                                          |
| char name      | 0x14 (20)      | `D2CHARSAVE_CHARNAME_OFFSET_109`           |
| **status**     | 0x24 (36)      | `D2CHARSAVE_STATUS_OFFSET_109` (= 0x24)    |
| **class**      | 0x28 (40)      | `D2CHARSAVE_CLASS_OFFSET_109` (= 0x28)     |
| **level**      | 0x2B (43)      | (level lives at status_offset_109 + 7)     |

Status byte bit values (ORIGINAL `d2charfile.h`, identical to on-disk D2 bits):
`INIT=0x01, HARDCORE=0x04, DEAD=0x08, EXPANSION=0x20, LADDER=0x40`.

---

## FINDING 1 — `extract_class` reads the STATUS byte instead of class (offset 36 vs 40)

- **Severity:** HIGH
- **Classification:** BUG

**Original ref** — `src/d2cs/d2charfile.h:31`
```c
#define D2CHARSAVE_CLASS_OFFSET_109   0x28   /* = 40 */
```
and `d2charfile.cpp:77` writes class at `D2CHARSAVE_CLASS_OFFSET_109`.

**v3 ref** — `src/protocol/d2save/src/codec.cpp:219-229`
```cpp
core::Result<uint8_t,...> D2SaveCodec::extract_class(std::span<const uint8_t> data) {
    ...
    // Character class is at offset 36
    return core::Result<uint8_t,...>(data[36]);
}
```

**Divergence:** Offset 36 (0x24) is the **status** byte, not the class. Class is
at offset 40 (0x28). The v3 struct-based `parse()` (codec.cpp:79, `char_class`
landing at sequential offset 40) is correct, but the standalone `extract_class`
helper — the one actually used by `character_persistence.cpp:81` to populate
`LoadCharacterResult::char_class` — reads the wrong byte. Every loaded character
gets its class set from the status flags byte (e.g. a hardcore-ladder char
reports class = 0x44 = garbage).

**Proposed fix:** read `data[40]` (guard `data.size() < 41`).

---

## FINDING 2 — `extract_level` reads the CLASS byte instead of level (offset 40 vs 43)

- **Severity:** HIGH
- **Classification:** BUG

**Original ref** — class is at 0x28 (40); level is 7 bytes past status_109
(0x24+7 = 0x2B = 43). In the v3 struct parse, `char_level` lands at sequential
offset 43 (codec.cpp:82), which is correct.

**v3 ref** — `src/protocol/d2save/src/codec.cpp:207-217`
```cpp
// Character level is at offset 40
return core::Result<uint8_t,...>(data[40]);
```

**Divergence:** Offset 40 (0x28) is the **class** byte. Level is at offset 43
(0x2B). `character_persistence.cpp:76` uses this helper, so every loaded char's
reported level equals its class id (1..7) instead of its real level.

**Proposed fix:** read `data[43]` (guard `data.size() < 44`).

---

## FINDING 3 — `is_hardcore` / `is_expansion` use wrong bit positions AND wrong offset semantics

- **Severity:** HIGH
- **Classification:** BUG

**Original ref** — `src/d2cs/d2charfile.h:46-48`
```c
#define D2CHARINFO_STATUS_FLAG_EXPANSION  0x20
#define D2CHARINFO_STATUS_FLAG_LADDER     0x40
#define D2CHARINFO_STATUS_FLAG_HARDCORE   0x04
```
Status byte is at offset 0x24 (36) for 1.09+.

**v3 ref** — `src/protocol/d2save/src/codec.cpp:231-255`
```cpp
// Expansion flag is bit 2 of char_status at offset 36
uint8_t char_status = data[36];
return ... ((char_status & 0x04) != 0);   // is_expansion
...
// Hardcore flag is bit 0 of char_status at offset 36
return ... ((char_status & 0x01) != 0);   // is_hardcore
```

**Divergence:** The *offset* (36) is correct for the status byte, but the *bit
masks* are wrong:
- `is_hardcore` tests `0x01` — that is the **INIT** bit, not hardcore (0x04).
- `is_expansion` tests `0x04` — that is the **HARDCORE** bit, not expansion (0x20).

Net effect: hardcore characters are never reported hardcore; init-flagged chars
are misreported as hardcore; expansion detection actually returns the hardcore
flag. Consumed by `character_persistence.cpp:86,91`.

**Proposed fix:** `is_hardcore` → `char_status & 0x04`; `is_expansion` →
`char_status & 0x20`. (Consider also exposing dead = `0x08`, ladder = `0x40`.)

---

## FINDING 4 — Shared `CharacterClass` enum has wrong ordinal mapping (sorceress/necromancer & paladin/barbarian swapped)

- **Severity:** HIGH
- **Classification:** BUG

**Original ref** — `src/bnetd/character.h:29-39`
```c
character_class_none,        // 0 (sentinel; D2 byte values are -1 from this)
character_class_amazon,      // amazon=0 on the wire
character_class_sorceress,   // 1
character_class_necromancer, // 2
character_class_paladin,     // 3
character_class_barbarian,   // 4
character_class_druid,       // 5
character_class_assassin     // 6
```
Confirmed by the d2cs newbie-template switch in `d2charfile.cpp:42-51,197-219`:
`amazon=0, sorceress=1, necromancer=2, paladin=3, barbarian=4, druid=5,
assassin=6`.

**v3 ref** — `src/domain/shared/.../d2_character_class.hpp:14-22`
```cpp
enum class CharacterClass : std::uint8_t {
    amazon = 0,
    necromancer,   // = 1  (should be sorceress)
    paladin,       // = 2  (should be necromancer)
    barbarian,     // = 3  (should be paladin)
    sorceress,     // = 4  (should be barbarian)
    druid,         // = 5
    assassin,      // = 6
};
```

**Divergence:** Sorceress (correct=1) and Necromancer (correct=2) are swapped;
Paladin (correct=3) and Barbarian (correct=4) are swapped. This enum is the
published-kernel value used by both `realm` (`CharacterStats::char_class`) and
`ladder` (`d2_ladder.hpp:136`). Any code that maps the raw `.d2s`/wire class byte
to this enum, or vice-versa, mislabels four of seven classes. A barbarian save
(byte 4) becomes `sorceress`; a paladin (byte 3) becomes `barbarian`; etc.

**Proposed fix:** reorder to `amazon=0, sorceress=1, necromancer=2, paladin=3,
barbarian=4, druid=5, assassin=6`.

---

## FINDING 5 — d2cs `CharacterClass` enum ALSO mis-ordered (different wrong order)

- **Severity:** HIGH
- **Classification:** BUG

**Original ref** — same canonical order as Finding 4
(`amazon=0, sorceress=1, necromancer=2, paladin=3, barbarian=4, druid=5,
assassin=6`).

**v3 ref** — `src/domain/d2cs/include/domain/d2cs/types.hpp:27-35`
```cpp
enum class CharacterClass : uint8_t {
    Amazon      = 0,
    Necromancer = 1,   // should be Sorceress
    Barbarian   = 2,   // should be Necromancer
    Sorceress   = 3,   // should be Paladin
    Paladin     = 4,   // should be Barbarian
    Druid       = 5,
    Assassin    = 6,
};
```

**Divergence:** A *third* inconsistent ordering. Positions 1-4 are a rotation:
Sorceress→1, Necromancer→2, Paladin→3, Barbarian→4 are all wrong. The d2cs
session handler casts the raw wire byte straight into this enum
(`src/app/d2cs/src/d2cs_session_handler.cpp:152`:
`info.class_ = static_cast<domain::d2cs::CharacterClass>(req.char_class);`),
so the create-character flow stores a misnamed class. Note this enum and the
shared-kernel enum (Finding 4) disagree with *each other* as well as with the
original — there is no single source of truth.

**Proposed fix:** align to the canonical order (ideally collapse onto the shared
`domain::CharacterClass` so only one definition exists).

---

## What MATCHES (verified correct)

- **`.d2s` signature** — v3 `D2S_SIGNATURE = 0xAA55AA55`
  (`codec.hpp:26`). Correct.
- **`.d2s` versions** — v3 `D2S_VERSION_109 = 87` (0x57), `D2S_VERSION_110 = 96`
  (0x60) (`codec.hpp:27-28`). Consistent with the original's
  `version >= 0x5C` (92) gate that selects the 1.09+ offsets; 87 and 96 are the
  real client version stamps and both fall in the 1.09/1.10 family. (Minor note:
  see Observation A.)
- **Char-name offset** — `extract_char_name` reads offset 20
  (`codec.cpp:190-192`) = `D2CHARSAVE_CHARNAME_OFFSET_109 = 0x14`. Correct.
- **Char-name length** — v3 limit 15 usable chars
  (`create_character.cpp:7,28`; `use_cases.hpp:42`) matches original
  `MAX_CHARNAME_LEN = 16` (incl. NUL) / `MIN_CHARNAME_LEN = 2`.
- **Char-list capacity** — v3 default `max_capacity = 8`
  (`character_list.hpp:41`) matches original `MAX_CHAR_PER_ACCT` /
  `maxchar = 8` (`conf/d2cs.conf.in:142`). (Minor note: v3 hardcodes the default
  rather than wiring the `maxchar` pref — Observation B.)
- **d2cs `CharacterFlags`** — `Hardcore=0x04, Died=0x08, Expansion=0x20,
  Ladder=0x40` (`domain/d2cs/types.hpp`) exactly match original
  `D2CHARINFO_STATUS_FLAG_*`. Correct (contrast with the codec's wrong masks in
  Finding 3).
- **Ladder/charlist reply status flags** — v3 `kLadderStatusDead=0x10,
  Hardcore=0x20, Expansion=0x40, Difficulty=0x0f00`
  (`protocol/d2cs/wire_types.hpp:97-100`) match original
  `LADDERSTATUS_FLAG_*` (`d2cs_protocol.h:276-279`). Correct. (These are the
  client-facing ladder byte values and are intentionally different from the
  on-disk `.d2s` status bits.)
- **d2cs<->d2dbs packet command codes** — all match
  (`protocol/d2dbs/wire_types.hpp` vs `d2dbs/dbspacket.h`):
  CONNECT=0x65, SAVE_DATA=0x30, GET_DATA=0x31, UPDATE_LADDER=0x32,
  CHAR_LOCK=0x33, ECHO=0x34; data subcodes CHARSAVE=0x01, PORTRAIT=0x02;
  result codes SUCCESS=0/FAILED=1/CHARLOCKED=2.
- **d2dbs packet fixed-field layout & sizes** — header (size16+type16+seqno32 =
  8 bytes) and every fixed-part size match the original byte-for-byte:
  SaveDataReq +4, SaveDataReply +6, GetDataReq +2, GetDataReply +16,
  UpdateLadder +16, CharLock +4, Echo +0. Field order/types also match.
- **charinfo magicword/version** — not re-implemented in v3 codec (v3 only
  parses raw `.d2s`, not the legacy `t_d2charinfo_file` summary), so
  `D2CHARINFO_MAGICWORD=0x12345678` / `D2CHARINFO_VERSION=0x00010000` have no v3
  counterpart to diverge. No bug; noted for completeness.

---

## Minor observations (not bugs, lower confidence)

- **Observation A** — v3 only accepts exactly version 87 or 96
  (`codec.cpp:56`), whereas the original accepts a *range* (`version >= 0x5C`)
  and even handles pre-1.09 saves via the legacy offsets
  (`D2CHARSAVE_*_OFFSET` at 0x18/0x22/0x08). v3 has no pre-1.09 path at all. If
  parity with old/odd-version saves is intended this is a gap; if v3 deliberately
  supports only 1.09/1.10 it is INTENTIONAL. Marked UNSURE.

- **Observation B** — v3 char-list cap is a hardcoded default (8) rather than
  reading the configurable `maxchar` pref the original exposes. Functionally
  equivalent at the default; a config-parity gap only. UNSURE/INTENTIONAL.

(none remaining — former Observation C was confirmed a real bug, see Finding 6.)

---

## FINDING 6 — `.d2s` checksum uses CRC32 instead of D2's rotate-left-add algorithm

- **Severity:** HIGH
- **Classification:** BUG

**Original ref** — `src/common/d2char_checksum.cpp:25-41`
```c
extern int d2charsave_checksum(unsigned char const * data, unsigned int len, unsigned int offset)
{
    int checksum = 0;
    for (i = 0; i < len; i++) {
        if (i >= offset && i < offset + sizeof(int)) ch = 0;  // zero the checksum field
        else ch = *data;
        ch += (checksum < 0);          // carry out of the sign bit
        checksum = 2 * checksum + ch;  // rotate-left-with-carry + add byte
        data++;
    }
    return checksum;
}
```
This is the real Blizzard `.d2s` checksum (left-rotate accumulate; the 4
checksum bytes are treated as zero while summing). Used by
`d2charfile.cpp:88-90, 396-397, 588-589`.

**v3 ref** — `src/protocol/d2save/src/codec.cpp:10-26, 147-180`
```cpp
constexpr uint32_t CRC32_POLY = 0xEDB88320;       // reflected CRC32 poly
uint32_t compute_crc32(std::span<const uint8_t> data) { ... }   // standard CRC32
...
bool D2SaveCodec::verify_checksum(...) { ... computed == stored_checksum; }
std::vector<uint8_t> D2SaveCodec::fix_checksum(...) { ... }      // writes CRC32 at off 8
```

**Divergence:** v3 computes a **reflected CRC32** (poly 0xEDB88320), a totally
different algorithm from D2's rotate-left-add. Consequences:
1. `verify_checksum` will report essentially every real `.d2s` as invalid, so
   `D2SaveFile::valid` is wrong (`parse()` sets `result.valid = verify_checksum`).
2. `fix_checksum` writes a CRC32 value the D2 client will reject as corrupt.
3. Wrong checksum-field offset semantics too: the original *zeros the 4
   checksum bytes in place* while accumulating; v3 instead *splices them out*
   (`temp = data[0..8) + data[12..end)`), shortening the buffer by 4 bytes and
   shifting every subsequent byte — so even if the algorithm matched it would
   still be computed over the wrong layout.

**Proposed fix:** replace `compute_crc32` with the rotate-left-add loop from
`d2char_checksum.cpp`, accumulating over the full buffer with the 4 checksum
bytes (at offset 8) treated as zero, and return/store the low 32 bits.
```
