# SPDX-License-Identifier: GPL-2.0-or-later
#
# Defines the `pvpgn_migrate` executable — a standalone CLI tool for
# migrating PvPGN account data between storage backends.
#
# Supported migrations
# --------------------
#   --from-plain <dir> --to-sqlite <db_path>
#       Reads legacy *.plain account files and writes them into a SQLite DB.
#
#   --from-plain <dir> --to-toml-file <dst_dir>
#       Reads legacy *.plain account files and writes TOML files per account.
#
# Dependencies
# ------------
#   pvpgn_infra_file    — flat_db_reader (parse_account_file, get_field, …)
#   pvpgn_infra_sqlite  — SQLiteUnitOfWorkFactory, SQLiteConnection
#   pvpgn_core          — core::Result, core::Error, domain types

cmake_minimum_required(VERSION 3.20)

add_executable(pvpgn_migrate
    main.cpp
    migrate_cli.cpp
    migrate_accounts.cpp
    migrate_config.cpp
)

# Apply v3 warning flags + C++20 standard
pvpgn_v3_apply_flags(pvpgn_migrate)

target_link_libraries(pvpgn_migrate
    PRIVATE
        pvpgn_infra_file
        pvpgn_infra_sqlite
        pvpgn_core
)

install(TARGETS pvpgn_migrate
    RUNTIME DESTINATION ${BIN_INSTALL_DIR}
)
