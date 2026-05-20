# d2cs Parity Harness Design (R83/R84 enabler)

## Problem

The legacy d2cs emitters `D2CS_CLIENT_LADDERREPLY` (`on_client_ladderreq` /
`on_client_charladderreq` / `d2cs_send_client_ladder`) and
`D2CS_CLIENT_CHARLISTREPLY` (`on_client_charlistreq*`) produce wire bytes
through a sequence of `packet_append_data`, `packet_append_string`,
`packet_set_size(rpacket, packet_get_size(rpacket) - 4)` tricks, and
mid-stream pagination. The exact byte layout depends on:

- `t_d2cs_client_ladderreply` packet header struct sizes (header + fields
  like `type/total_len/curr_len/cont_len`).
- `t_d2cs_client_ladderheader` (8 bytes: `u16 start_pos + u16 u1 +
  u32 count1`) appended into the payload of the first packet only.
- `t_d2cs_client_ladderinfoheader` (4 bytes: `u32 count2`) appended into
  every packet; `count2 = count` only on the first packet, otherwise 0.
- The `packet_set_size(...) - 4` truncation hack on the first packet's
  final reported size (paired with `curr_len -= 4`).
- The `D2LADDER_*` enum ordering, the per-packet count of 14
  `t_d2cs_client_ladderinfo` entries, and pagination via
  `cont_len += curr_len` carried across packets.

For `CHARLISTREPLY` the byte stream is two passes:

1. The first pass walks the charinfo directory (or builds it on
   `OpenError`) and appends `charname` + `portrait` zero-terminated
   strings, in either ASC or DESC order, for `n` characters bounded by
   `maxchar` (from `prefs_get_maxchar`).
2. `maxchar` in the reply header is the *remaining slots* (or 0 when
   `allow_newchar=false` or directory creation failed), and
   `currchar / currchar2` are both set to the realized count `n` after
   the walk.

Without a deterministic golden capture there is no safe way to write a v3
bridge that re-emits these byte streams.

## Goals

1. Produce a small, hermetic harness that drives the **legacy** emitters
   with fully synthetic inputs, captures the resulting byte stream from
   `conn_push_outqueue`, and writes it to a golden file under
   `tests/data/d2cs-parity/`.
2. Provide a **replay** test in `tests/unit/integration/legacy_d2cs/`
   that loads the golden file and compares it byte-for-byte to a v3
   emitter (initially: the existing `pvpgn_v3_d2cs_send_ladderreply_try`
   / `pvpgn_v3_d2cs_send_charlistreply_try` stubs, which currently
   return `0` so the test will start in a known-failing state).
3. Once the v3 bridge is written to match the golden, the legacy call
   site can be gated with the same `PVPGN_V3_D2CS_INTEGRATION` pattern
   used by `pvpgn_v3_d2cs_send_motdreply` (return 1 = handled, 0 =
   fallback) and the legacy fallback removed only after the test goes
   green across all golden cases.

## Architecture

### Capture side (legacy harness)

- Live in `tests/integration/legacy_d2cs/parity_capture/`, built only
  when `PVPGN_BUILD_LEGACY=ON` and `WITH_D2CS=ON` so it shares the
  legacy d2cs translation units that already exist in the legacy
  binary.
- Extracts the body of `d2cs_send_client_ladder` and the
  `on_client_charlistreq` payload-walk into thin re-entrant helpers
  (or links them directly), feeding:
  - a fake `t_connection*` whose `conn_push_outqueue` is replaced with
    a capture sink that appends `(packet_type, packet_get_size,
    raw_bytes)` to an `std::vector<CapturedPacket>`;
  - a stub `d2ladder_get_ladder` that returns a constant array of
    `t_d2cs_client_ladderinfo` (e.g. 3, 14, 15, 28, 29 entries to cover
    1-, 1-full-, 2-, 2-full-, 3-packet pagination);
  - a stub `prefs_get_ladderlist_count` / `prefs_get_maxchar` /
    `prefs_get_charlist_sort_order` returning deterministic constants;
  - a stub `Directory` reading from an `std::vector<std::string>` of
    charnames.
- Output: one file per scenario, e.g.
  `tests/data/d2cs-parity/ladderreply__type=STD_OVERALL__from=0__count=14.bin`.
  Header line `# packet_type=0x07 size=N\n` followed by raw bytes;
  trailing `\n# packet_type=0x07 size=M\n` for each continuation
  packet in the same file.

### Replay side (v3 unit test)

- Lives in `tests/unit/integration/legacy_d2cs/parity_replay/`.
- Catch2 test that:
  1. Parses the capture file into `std::vector<CapturedPacket>`.
  2. Constructs a v3 emitter writer
     (`pvpgn::protocol::d2cs::ladderreply::Encoder`,
     `pvpgn::protocol::d2cs::charlistreply::Encoder` -- both to be
     written) with the same synthetic inputs.
  3. Asserts that each emitted packet matches the captured one byte
     for byte, including the `-4` truncation hack and the `count2 = 0`
     on continuation packets.

### Bridge wiring

Same pattern as every other v3 bridge in this tree:

```cpp
extern "C" int pvpgn_v3_d2cs_send_ladderreply_try(
    void* conn, unsigned char type, unsigned short from) noexcept;
```

inside `integration_legacy_d2cs_linked`, called from
`d2cs_send_client_ladder` under `#ifdef PVPGN_V3_D2CS_INTEGRATION` and
returning 1 (handled) only after `parity_replay` proves byte equality.

## Scenarios to capture

### LADDERREPLY

| Case | type            | from | total ladder count | expected packets |
|------|-----------------|------|--------------------|------------------|
| L1   | STD_OVERALL     | 0    | 1                  | 1                |
| L2   | STD_OVERALL     | 0    | 14                 | 1                |
| L3   | STD_OVERALL     | 0    | 15                 | 2                |
| L4   | EXP_HC_OVERALL  | 10   | 28                 | 2                |
| L5   | EXP_STD_OVERALL | 0    | 29                 | 3                |
| L6   | HC_OVERALL      | 0    | 0                  | 1 (empty reply)  |
| L7   | STD_OVERALL     | 5    | 100                | 8                |

Each `t_d2cs_client_ladderinfo` is filled with a deterministic byte
pattern derived from the index (e.g. `memset(idx | 0x80)`) so a single
diff line pinpoints the exact mismatching field.

### CHARLISTREPLY

| Case | sort | charnames                       | maxchar | allow_newchar | dir openerror |
|------|------|---------------------------------|---------|---------------|---------------|
| C1   | ASC  | []                              | 8       | true          | false         |
| C2   | ASC  | ["alice"]                       | 8       | true          | false         |
| C3   | ASC  | ["alice","bob"]                 | 8       | true          | false         |
| C4   | DESC | ["alice","bob"]                 | 8       | true          | false         |
| C5   | ASC  | ["a","b","c","d","e","f","g","h"]| 8      | true          | false         |
| C6   | ASC  | ["alice","bob"]                 | 2       | true          | false         |
| C7   | ASC  | ["alice"]                       | 8       | false         | false         |
| C8   | ASC  | []                              | 8       | true          | true          |

Each `charinfo` is built with a deterministic portrait (16 bytes of
`0xC1..0xD0`) and an explicit `header.charname`.

## Risks / open questions

1. The legacy code holds `t_packet` allocations via the `packet_create`
   pool which itself requires `eventlog` and `psock` initialization.
   The harness will need a small `legacy_d2cs_test_init()` shim that
   only sets up the bare minimum (the same one used by the other
   `tests/integration/legacy_d2cs/*` bridges already - check
   `send_packet_bridge.cpp` for the established pattern).
2. The `-4` truncation: needs careful inspection of which trailing
   bytes are discarded; the most likely answer is that the
   `t_d2cs_client_ladderreply` header struct includes 4 padding bytes
   the wire protocol does not expect. The capture will tell us
   definitively.
3. `prefs_get_*` calls without `prefs_set_*` counterparts may require
   stubbing via `--wrap` (gcc/ld) or a tiny `prefs_test.cpp` link
   shadow. Confirm one of these works against the Alpine toolchain
   before committing to a path.

## Next-round acceptance criteria

- All 7 LADDERREPLY scenarios and all 8 CHARLISTREPLY scenarios captured
  into `tests/data/d2cs-parity/`.
- `parity_replay` test compiles and is registered in the v3 docker
  build (initially expected to fail until the v3 encoders are written).
- Design doc updated with any deviations discovered during capture.

After that, the actual encoders + bridges (R83/R84 proper) become a
straightforward "implement until parity test goes green" exercise.
