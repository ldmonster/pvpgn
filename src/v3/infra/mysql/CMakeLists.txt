# SPDX-License-Identifier: GPL-2.0-or-later
# MySQL/MariaDB persistence backend (optional)

option(PVPGN_V3_WITH_MYSQL "Build with MySQL/MariaDB support" OFF)

if(PVPGN_V3_WITH_MYSQL)
    # MySQL header-only include for now (stub implementations)
    add_library(infra_mysql INTERFACE)
    target_include_directories(infra_mysql
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    # Link dependencies
    target_link_libraries(infra_mysql
        INTERFACE
        core
        application_ports
    )

    # TODO: Find MySQL client library
    # find_package(MySQL REQUIRED)
    # target_link_libraries(infra_mysql INTERFACE MySQL::MySQL)

    # Export the target
    set(PVPGN_V3_MYSQL_FOUND TRUE PARENT_SCOPE)
endif()
