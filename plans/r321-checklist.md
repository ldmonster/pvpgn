# R321 — `pvpgn-migrate` tool skeleton

## Checklist
- [x] Read `src/v3/app/bnetd/src/main.cpp` and `src/v3/app/bnetd/CMakeLists.txt` for reference
- [x] Read `src/v3/CMakeLists.txt` to understand where to add the new subdirectory
- [x] Created `src/v3/app/pvpgn-migrate/` directory
- [x] Created `src/v3/app/pvpgn-migrate/main.cpp` with CLI argument parsing:
  - `--from-plain <dir>` option
  - `--to-sqlite <db_path>` option
  - `--to-toml-file <dst_dir>` option
  - `--help` / `-h` option
  - Usage/help printed when no arguments or `--help` given
  - Dispatch to `migrate_plain_to_sqlite()` or `migrate_plain_to_toml()` stubs
  - Exit code 0 on success, 1 on error
  - `std::cerr` for errors, `std::cout` for progress
- [x] Created `src/v3/app/pvpgn-migrate/CMakeLists.txt`:
  - Defines `pvpgn_migrate` executable
  - Links against `pvpgn_infra_file`, `pvpgn_infra_sqlite`, `pvpgn_core`
  - Applies `pvpgn_v3_apply_flags` for C++20 + warnings
- [x] Added `add_subdirectory(app/pvpgn-migrate)` to `src/v3/CMakeLists.txt`

## Result
Created the `pvpgn-migrate` standalone CLI tool skeleton at
`src/v3/app/pvpgn-migrate/`. The tool parses `--from-plain`, `--to-sqlite`,
and `--to-toml-file` flags, validates argument combinations, and dispatches
to the appropriate migration function. The CMake target `pvpgn_migrate` links
against `pvpgn_infra_file`, `pvpgn_infra_sqlite`, and `pvpgn_core`. The
subdirectory is wired into `src/v3/CMakeLists.txt` after the `app/d2dbs`
entry.
