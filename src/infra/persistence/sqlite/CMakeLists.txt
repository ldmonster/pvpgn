# R316: infra/persistence/sqlite is DEPRECATED.
# The canonical SQLite implementation is pvpgn_infra_sqlite (infra/sqlite/).
# This subdirectory is intentionally disabled. Do not re-enable it.
# All callers must use: #include "infra/sqlite/account_repository.hpp"
message(STATUS "infra/persistence/sqlite: DEPRECATED — skipped (use pvpgn_infra_sqlite)")
