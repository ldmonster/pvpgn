cmake_minimum_required(VERSION 3.20)

add_library(pvpgn_discovery STATIC
    src/service_endpoint.cpp
    src/service_registry.cpp
    src/dns_service_registry.cpp
)

target_include_directories(pvpgn_discovery PUBLIC include)
target_link_libraries(pvpgn_discovery PUBLIC pvpgn_core)
target_compile_features(pvpgn_discovery PUBLIC cxx_std_20)

if(UNIX)
    target_link_libraries(pvpgn_discovery PRIVATE resolv)
endif()
