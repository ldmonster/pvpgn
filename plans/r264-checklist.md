# R264 — ladder: New `application/ladder/` Bounded Context

## Status: ✅ COMPLETE

---

### New bounded context: `src/v3/application/ladder/`

- [x] `include/application/ladder/recompute_ladder.hpp` — `RecomputeLadderCommand`, `RecomputeLadderResult`, `RecomputeLadder` use case
- [x] `include/application/ladder/get_ladder_entry.hpp` — `GetLadderEntryQuery`, `LadderEntryResult`, `GetLadderEntry` use case
- [x] `include/application/ladder/get_ladder_page.hpp` — `GetLadderPageQuery`, `LadderPage`, `GetLadderPage` use case
- [x] `src/recompute_ladder.cpp` — validates ladder_id, fetches top 100k entries, sorts by wins desc (ties by rating desc), saves back, returns count
- [x] `src/get_ladder_entry.cpp` — validates ladder_id, looks up account by ID, calls `get_rank(account.name().canonical())`, finds matching entry
- [x] `src/get_ladder_page.cpp` — validates page ≥ 1 and page_size ∈ [1,100], fetches all entries, computes slice, resolves account names
- [x] `CMakeLists.txt` — `pvpgn_application_ladder` static library target

### Tests: `tests/unit/application/ladder/`

- [x] `recompute_ladder_test.cpp` — 3 test cases: happy path (entries sorted), empty ladder_id → `InvalidArgument`, no entries → 0 updated
- [x] `get_ladder_entry_test.cpp` — 2 test cases: happy path (entry found with rank/stats), account not in ladder → `NotFound`
- [x] `get_ladder_page_test.cpp` — 3 test cases: happy path page 1 of 3, page_size > 100 → `InvalidArgument`, page beyond total → empty entries
- [x] `CMakeLists.txt` — three `pvpgn_v3_add_test` targets with inline `FakeLadderRepository` / `FakeAccountRepository`

### Build wiring

- [x] `src/v3/application/CMakeLists.txt` — `add_subdirectory(ladder)` added after `add_subdirectory(tournament)`
- [x] `tests/unit/application/CMakeLists.txt` — guarded `if(TARGET pvpgn_application_ladder) add_subdirectory(ladder) endif()`

---

### Design notes

- All three use cases follow constructor injection; no global state.
- `[[nodiscard]]` on every `execute()` method.
- `LadderEntryResult` is defined in `get_ladder_entry.hpp` and reused by `get_ladder_page.hpp` via `#include`.
- `get_rank()` takes `std::string_view account_name`, so `GetLadderEntry` first resolves the `AccountId` → `Account` → `account.name().canonical()`.
- Tests use inline fakes only; no infra repositories are linked.
