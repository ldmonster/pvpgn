pvpgn_v3_add_test(test_infra_persistence_connection_string
    SOURCES connection_string_test.cpp
    DEPS    infra_persistence)

# Plan 07: consolidated account-ban repository over IDbDriver, tested with a
# recording fake driver (no sqlite needed — runs in any environment).
pvpgn_v3_add_test(test_infra_persistence_sql_account_ban_repository
    SOURCES sql_account_ban_repository_test.cpp
    DEPS    infra_persistence)

pvpgn_v3_add_test(test_infra_persistence_sql_realm_repository
    SOURCES sql_realm_repository_test.cpp
    DEPS    infra_persistence)

pvpgn_v3_add_test(test_infra_persistence_sql_friend_list_repository
    SOURCES sql_friend_list_repository_test.cpp
    DEPS    infra_persistence)

pvpgn_v3_add_test(test_infra_persistence_sql_clan_repository
    SOURCES sql_clan_repository_test.cpp
    DEPS    infra_persistence)

pvpgn_v3_add_test(test_infra_persistence_sql_ip_ban_repository
    SOURCES sql_ip_ban_repository_test.cpp
    DEPS    infra_persistence)

pvpgn_v3_add_test(test_infra_persistence_sql_game_repository
    SOURCES sql_game_repository_test.cpp
    DEPS    infra_persistence)

pvpgn_v3_add_test(test_infra_persistence_sql_ladder_repository
    SOURCES sql_ladder_repository_test.cpp
    DEPS    infra_persistence)

# Plan 07: consolidated driver-parameterized account repository over the
# SQLite IDbDriver (in-memory). Needs the sqlite backend lib for the driver
# + connection + migrations.
pvpgn_v3_add_test(test_infra_persistence_sql_account_repository
    SOURCES sql_account_repository_test.cpp
    DEPS    infra_persistence pvpgn_infra_sqlite)
