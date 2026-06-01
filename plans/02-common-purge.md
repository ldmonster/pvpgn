# 02 — `src/common/` Purge

## What

Delete or relocate every file under `src/common/` until only true
wire-protocol constants remain (`bnet_protocol/`, `*_protocol.h`,
`field_sizes.h`).

## Why

- 80+ files in `src/common/` are pre-C++20 utilities only kept alive by
  the legacy tree. They block C++23 uplift (plan 09) and the async I/O
  rewrite (plan 06).
- Every `src/common/` include in a v3 source is a layering violation
  waiting to happen.
- Custom containers (`hashtable`, `list`, `queue`, `elist`) hide
  iterator-invalidation and allocation bugs that disappear under
  `std::` equivalents.

## Prerequisites

- Plan 03 (legacy `bnetd` strangler) must reach ≥ 80% LOC reduction
  first — many of these utilities are only consumed by the legacy tree.

## Concrete steps

1. **Inventory.** Generate `planstwo/inventory/common-consumers.csv`:
   for each file in `src/common/`, list its v3 consumers (anything
   outside `src/common/` and `src/integration/legacy_*`).
2. **Classify** each file into one of:
   - `delete` — only legacy consumers.
   - `move-core` — has v3 consumers and is platform-agnostic. Move to
     `src/core/` with a thin re-export shim.
   - `move-infra-net` — fdwatch and friends. Move under
     `src/infra/net/`.
   - `keep-wire` — protocol constants. Move to `src/protocol/<family>/`.
3. **Per facet, in this order:**
   - `xstring`, `util*`, `tag`, `bn_type`, `bigint` → fold into
     `core/strings/`, `core/encoding/`, `core/numeric/`.
   - `hashtable`, `list`, `queue`, `elist` → delete; replace call sites
     with `std::unordered_map`, `std::vector`, `std::deque`,
     `std::list`.
   - `eventlog` → already shimmed; delete the shim once Plan 03 lands.
   - `fdwatch*`, `network`, `rlimit`, `give_up_root_privileges` →
     `infra/net/` / `infra/process/`.
   - `bnethash`, `wolhash`, `bnetsrp3`, `bnethashconv` → see plan 08.
   - `pugixml`, `scoped_*`, `asnprintf`, `fmt_compat` — already moved
     or deleted in wave one; verify and close out.
   - `*_protocol.h` headers → `src/protocol/<family>/include/...`.
4. **CMake cleanup.** Remove `src/common/CMakeLists.txt` once empty.
   Drop the `pvpgn_common` target.
5. **Layering exception cleanup.** Remove every `src/common/` entry
   from `cmake/layering_exceptions.txt`.

## Acceptance criteria

- [ ] `src/common/` contains only protocol headers and a README
      explaining why those stayed.
- [ ] `pvpgn_common` CMake target is deleted.
- [ ] `git grep -l 'include "common/' src/` returns nothing under
      `src/{core,domain,application,infra,integration/{bnet,irc,telnet,wol}}`.
- [ ] Layering check passes with zero `src/common/` exceptions.
- [ ] Sanitizer matrix green.

## Risks

- Iterator-invalidation bugs masked by `hashtable` may surface when
  swapping in `std::unordered_map`. Mitigate with unit tests written
  before the swap.
- `eventlog` is the load-bearing logging API for legacy code; only
  delete after plan 03 fully retires its callers.

## Out of scope

- Modernizing the wire-protocol constants beyond moving them.
- Rewriting the SRP / hash functions on the wire (plan 08 covers the
  password-at-rest side only).
