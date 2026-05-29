# SPDX-License-Identifier: GPL-2.0-or-later
# MySQL/MariaDB persistence backend (optional, conditionally compiled)

# Try to find MySQL client library (libmysqlclient / mariadb-connector-c)
find_package(MySQL QUIET)
if(NOT MySQL_FOUND)
    # Fallback: try pkg-config for mysqlclient / mariadb
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(MYSQLCLIENT QUIET mysqlclient)
        if(NOT MYSQLCLIENT_FOUND)
            pkg_check_modules(MYSQLCLIENT QUIET mariadb)
        endif()
    endif()
endif()

# Determine whether MySQL support is available
if(MySQL_FOUND OR MYSQLCLIENT_FOUND)
    set(_pvpgn_mysql_available TRUE)
    message(STATUS "MySQL/MariaDB client library found — building pvpgn_infra_mysql with real support")
else()
    set(_pvpgn_mysql_available FALSE)
    message(STATUS "MySQL/MariaDB client library NOT found — pvpgn_infra_mysql will be a stub (throws at runtime)")
endif()

# Always define the target so downstream CMakeLists can link against it
# unconditionally; the stub path compiles without any MySQL headers.
add_library(pvpgn_infra_mysql STATIC
    include/infra/mysql/connection.hpp
    include/infra/mysql/account_repository.hpp
    include/infra/mysql/unit_of_work.hpp
    include/infra/mysql/unit_of_work_factory.hpp
    src/connection.cpp
    src/account_repository.cpp
    src/unit_of_work.cpp
    src/unit_of_work_factory.cpp
)

target_include_directories(pvpgn_infra_mysql
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pvpgn_infra_mysql
    PUBLIC
        pvpgn_core
        pvpgn_application
        pvpgn_infra_inmemory
)

if(_pvpgn_mysql_available)
    target_compile_definitions(pvpgn_infra_mysql PUBLIC PVPGN_V3_WITH_MYSQL)

    if(MySQL_FOUND)
        target_include_directories(pvpgn_infra_mysql PRIVATE ${MySQL_INCLUDE_DIRS})
        target_link_libraries(pvpgn_infra_mysql PRIVATE ${MySQL_LIBRARIES})
    elseif(MYSQLCLIENT_FOUND)
        target_include_directories(pvpgn_infra_mysql PRIVATE ${MYSQLCLIENT_INCLUDE_DIRS})
        target_link_libraries(pvpgn_infra_mysql PRIVATE ${MYSQLCLIENT_LIBRARIES})
    endif()
endif()
