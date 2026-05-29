# SPDX-License-Identifier: GPL-2.0-or-later

if(TARGET pvpgn_infra_migrations)
    pvpgn_v3_add_test(test_infra_migrations
        SOURCES
            migration_runner_test.cpp
        DEPS
            pvpgn_infra_migrations
    )
endif()
