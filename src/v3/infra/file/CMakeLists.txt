add_library(pvpgn_infra_file STATIC
    include/infra/file/flat_db_reader.hpp
    include/infra/file/account_repository.hpp
    include/infra/file/ip_ban_repository.hpp
    include/infra/file/unit_of_work.hpp
    include/infra/file/unit_of_work_factory.hpp
    src/flat_db_reader.cpp
    src/account_repository.cpp
    src/ip_ban_repository.cpp
    src/unit_of_work.cpp
    src/unit_of_work_factory.cpp
)

target_include_directories(pvpgn_infra_file
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pvpgn_infra_file
    PUBLIC
        pvpgn_core
        pvpgn_application
        pvpgn_infra_inmemory
)
