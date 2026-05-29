# SPDX-License-Identifier: GPL-2.0-or-later
# Runtime service library for shared service host infrastructure

add_library(pvpgn_runtime STATIC
    include/runtime/service_host.hpp
    include/runtime/composition_root.hpp
    include/runtime/peer_link.hpp
    include/runtime/capability_token.hpp
    include/runtime/service_config_loader.hpp
    include/runtime/service_logger.hpp
    include/runtime/service_metrics.hpp
    include/runtime/health_check.hpp
    src/service_host.cpp
    src/cli.cpp
    src/daemonize.cpp
    src/crash_handler.cpp
    src/peer_link.cpp
    src/capability_token.cpp
    src/service_config_loader.cpp
    src/service_logger.cpp
    src/service_metrics.cpp
    src/health_check.cpp
)

if(WIN32)
    target_sources(pvpgn_runtime PRIVATE src/win_service.cpp)
endif()

target_include_directories(pvpgn_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(pvpgn_runtime
    PUBLIC
        pvpgn_core
        OpenSSL::SSL
        OpenSSL::Crypto
        spdlog::spdlog
        Boost::asio
        Boost::system
)

if(WIN32)
    target_link_libraries(pvpgn_runtime PRIVATE dbghelp)
endif()

# Enable C++20 features
target_compile_features(pvpgn_runtime PUBLIC cxx_std_20)

# Add subdirectories for service implementations
# (d2cs, d2dbs, etc. will be added later)
