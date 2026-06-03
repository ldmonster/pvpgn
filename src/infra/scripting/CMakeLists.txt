# SPDX-License-Identifier: GPL-2.0-or-later
# src/v3/infra/scripting/CMakeLists.txt
#
# pvpgn_infra_scripting — sol2 / Lua 5.4 adapter + ScriptHost port (R348)
#                         Lua API v2 surface + legacy shim (R349)

# ---- Option -----------------------------------------------------------------
option(PVPGN_V3_WITH_LUA "Enable Lua scripting via sol2" ON)

if(NOT PVPGN_V3_WITH_LUA)
    message(STATUS "pvpgn_infra_scripting: Lua scripting DISABLED (PVPGN_V3_WITH_LUA=OFF)")
    return()
endif()

# ---- Lua 5.4 ----------------------------------------------------------------
find_package(Lua 5.4 REQUIRED)

# ---- sol2 (header-only) -----------------------------------------------------
# Try to find an installed copy first; fall back to FetchContent.
find_path(SOL2_INCLUDE_DIR sol/sol.hpp
    HINTS
        ${CMAKE_SOURCE_DIR}/third_party/sol2/include
        /usr/include
        /usr/local/include
    NO_DEFAULT_PATH
)

if(NOT SOL2_INCLUDE_DIR)
    message(STATUS "pvpgn_infra_scripting: sol2 not found locally — fetching via FetchContent")
    include(FetchContent)
    FetchContent_Declare(sol2
        GIT_REPOSITORY https://github.com/ThePhD/sol2.git
        GIT_TAG        v3.3.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(sol2)
    set(SOL2_INCLUDE_DIR "${sol2_SOURCE_DIR}/include")
endif()

message(STATUS "pvpgn_infra_scripting: sol2 include dir = ${SOL2_INCLUDE_DIR}")

# ---- Library ----------------------------------------------------------------
pvpgn_v3_add_library(pvpgn_infra_scripting STATIC
    SOURCES
        src/sol2_script_host.cpp   # R348 — IScriptHost implementation
        src/lua_api_v2.cpp         # R349 — pvpgn.* Lua API surface
        src/legacy_shim.cpp        # R349 — bnetd_* → pvpgn.* compatibility shim
)

target_include_directories(pvpgn_infra_scripting
    PUBLIC
        include
    PRIVATE
        src
)

# sol2 / Lua headers are 3rd-party — mark SYSTEM so our strict
# -Werror=sign-conversion / -Werror=conversion / -Wtemplate-body
# do not flag bugs inside those headers.
target_include_directories(pvpgn_infra_scripting SYSTEM PRIVATE
    ${SOL2_INCLUDE_DIR}
    ${LUA_INCLUDE_DIR}
)

target_link_libraries(pvpgn_infra_scripting
    PUBLIC  application_ports
    PRIVATE ${LUA_LIBRARIES}
)

# sol2 v3.3.0 hits gcc 14's new -Wtemplate-body diagnostic in
# `sol/optional_implementation.hpp`. SYSTEM include suppresses most
# warnings but not template-body, so silence it explicitly.
#
# NB: probe the *positive* -Wtemplate-body, not -Wno-template-body. GCC
# silently accepts unknown -Wno-* options until a diagnostic actually fires,
# so check_cxx_compiler_flag(-Wno-template-body) reports success even on
# GCC 13 (which has no such warning) — and then -Wno-error=template-body
# hard-errors at compile time ("no option '-Wtemplate-body'"). The positive
# form fails the check correctly on compilers older than GCC 14.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(-Wtemplate-body
        PVPGN_HAVE_WTEMPLATE_BODY)
    if(PVPGN_HAVE_WTEMPLATE_BODY)
        target_compile_options(pvpgn_infra_scripting PRIVATE
            -Wno-template-body -Wno-error=template-body)
    endif()
endif()

pvpgn_v3_target_warnings(pvpgn_infra_scripting)

message(STATUS "pvpgn_infra_scripting: Lua scripting ENABLED (Lua ${LUA_VERSION_STRING})")
