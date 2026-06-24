# SPDX-License-Identifier: GPL-2.0-or-later

if(TARGET pvpgn_infra_file)
    pvpgn_v3_add_test(test_infra_file
        SOURCES
            file_account_repository_test.cpp
        DEPS
            pvpgn_infra_file
            pvpgn_application
    )

    pvpgn_v3_add_test(test_infra_file_ip_ban
        SOURCES
            file_ip_ban_repository_test.cpp
        DEPS
            pvpgn_infra_file
            pvpgn_application
    )
endif()
