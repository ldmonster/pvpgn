pvpgn_v3_add_test(test_infra_persistence_connection_string
    SOURCES connection_string_test.cpp
    DEPS    infra_persistence)

# Plan 07: consolidated driver-parameterized account repository over the
# SQLite IDbDriver (in-memory). Needs the sqlite backend lib for the driver
# + connection + migrations.
pvpgn_v3_add_test(test_infra_persistence_sql_account_repository
    SOURCES sql_account_repository_test.cpp
    DEPS    infra_persistence pvpgn_infra_sqlite)
