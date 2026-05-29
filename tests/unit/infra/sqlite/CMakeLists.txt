# SPDX-License-Identifier: GPL-2.0-or-later

if(TARGET pvpgn_infra_sqlite AND TARGET pvpgn_infra_migrations)
    pvpgn_v3_add_test(test_infra_sqlite
        SOURCES
            sqlite_account_repository_test.cpp
        DEPS
            pvpgn_infra_sqlite
            pvpgn_infra_migrations
            pvpgn_application
    )
endif()
