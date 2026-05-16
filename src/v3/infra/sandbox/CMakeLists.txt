cmake_minimum_required(VERSION 3.20)

# Check for AppArmor support
find_library(APPARMOR_LIB apparmor)
if(APPARMOR_LIB)
    set(HAVE_APPARMOR TRUE)
    message(STATUS "AppArmor library found: ${APPARMOR_LIB}")
else()
    message(STATUS "AppArmor library not found - AppArmor confinement disabled")
endif()

add_library(pvpgn_sandbox STATIC
    src/seccomp_filter.cpp
    src/apparmor_confinement.cpp
    src/plugin_sandbox.cpp
)

target_include_directories(pvpgn_sandbox
    PUBLIC include
)

target_link_libraries(pvpgn_sandbox
    PUBLIC pvpgn_core
)

if(HAVE_APPARMOR)
    target_compile_definitions(pvpgn_sandbox PRIVATE HAVE_APPARMOR=1)
    target_link_libraries(pvpgn_sandbox PRIVATE ${APPARMOR_LIB})
endif()

target_compile_features(pvpgn_sandbox PUBLIC cxx_std_20)

# Seccomp is Linux-only
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(pvpgn_sandbox PRIVATE PVPGN_LINUX=1)
endif()
