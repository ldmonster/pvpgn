add_library(pvpgn_infra_migrations STATIC
    include/infra/migrations/migration_runner.hpp
    include/infra/migrations/all_migrations.hpp
    src/migration_runner.cpp
    src/all_migrations.cpp
)

target_include_directories(pvpgn_infra_migrations
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pvpgn_infra_migrations
    PUBLIC
        pvpgn_core
)
