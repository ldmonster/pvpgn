# SPDX-License-Identifier: GPL-2.0-or-later

pvpgn_v3_add_test(test_infra_inmemory
    SOURCES
        in_memory_unit_of_work_test.cpp
        in_memory_clan_repository_test.cpp
        in_memory_ladder_repository_test.cpp
    DEPS
        infra_inmemory
        application_ports
)
