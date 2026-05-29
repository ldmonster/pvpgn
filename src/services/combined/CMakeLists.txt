cmake_minimum_required(VERSION 3.20)

option(PVPGN_SINGLE_BINARY "Build all services into a single binary" OFF)

if(PVPGN_SINGLE_BINARY)
    message(STATUS "Single-binary mode enabled")

    add_library(pvpgn_combined_service STATIC
        src/combined_composition.cpp
    )

    target_include_directories(pvpgn_combined_service PUBLIC include)

    target_link_libraries(pvpgn_combined_service
        PUBLIC
            pvpgn_core
            pvpgn_runtime
            pvpgn_discovery
    )

    target_compile_features(pvpgn_combined_service PUBLIC cxx_std_20)
    target_compile_definitions(pvpgn_combined_service PUBLIC PVPGN_SINGLE_BINARY=1)

    # Single-binary main executable
    add_executable(pvpgn
        src/main_combined.cpp
    )
    target_link_libraries(pvpgn PRIVATE pvpgn_combined_service)
endif()
