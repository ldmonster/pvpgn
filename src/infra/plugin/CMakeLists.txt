# SPDX-License-Identifier: GPL-2.0-or-later
# src/v3/infra/plugin/CMakeLists.txt
#
# pvpgn_infra_plugin — native plugin loader (C ABI 1.0) + optional seccomp sandbox.

# ---- R347: seccomp sandbox option -------------------------------------------
option(PVPGN_V3_WITH_SECCOMP
    "Enable seccomp-BPF sandboxing for native plugins (Linux only)"
    OFF)

# ---- Core plugin loader library ---------------------------------------------
pvpgn_v3_add_library(pvpgn_infra_plugin STATIC
    src/plugin_loader.cpp
    src/sandbox.cpp
)

target_include_directories(pvpgn_infra_plugin
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(pvpgn_infra_plugin
    PUBLIC pvpgn_core
)

# POSIX: link libdl for dlopen/dlsym/dlclose
if(UNIX)
    target_link_libraries(pvpgn_infra_plugin PUBLIC ${CMAKE_DL_LIBS})
endif()

# ---- R347: optional seccomp linkage -----------------------------------------
if(PVPGN_V3_WITH_SECCOMP AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSECCOMP REQUIRED libseccomp)

    target_compile_definitions(pvpgn_infra_plugin
        PRIVATE PVPGN_V3_SECCOMP_ENABLED=1
    )
    target_include_directories(pvpgn_infra_plugin
        PRIVATE ${LIBSECCOMP_INCLUDE_DIRS}
    )
    target_link_libraries(pvpgn_infra_plugin
        PUBLIC ${LIBSECCOMP_LIBRARIES}
    )
    message(STATUS "pvpgn_infra_plugin: seccomp sandbox ENABLED")
else()
    message(STATUS "pvpgn_infra_plugin: seccomp sandbox DISABLED")
endif()

pvpgn_v3_target_warnings(pvpgn_infra_plugin)
