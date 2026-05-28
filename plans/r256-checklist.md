# R256 — CI domain purity script

## Checklist

- [x] Created `scripts/check_domain_purity.sh`
- [x] Script checks 10 forbidden patterns
- [x] Script exits 0 on clean domain, 1 on violations
- [x] Script is executable (`chmod +x`)
- [x] Script passes against current `src/v3/domain/` (zero violations)
- [x] Script accepts optional path argument (default: `src/v3/domain`)

## Forbidden Patterns Checked

1. `#include "infra/..."` or `#include <infra/...>` — no infra deps in domain
2. `#include <iostream>` — no I/O streams
3. `#include <fstream>` — no file streams
4. `#include <cstdio>` — no C stdio
5. `#include <ctime>` — no C time (use core::SystemTime)
6. `std::cout/cerr/cin` — no direct I/O
7. `printf/fprintf/sprintf/snprintf` — no C-style I/O
8. `system_clock::now()` — no direct clock calls (inject time)
9. `static .* g_` — no global mutable state
10. `spdlog::` — no logging library in domain

## Exit Criterion

`bash scripts/check_domain_purity.sh` exits 0 with "✅ Domain purity check PASSED".

## Status: GREEN
