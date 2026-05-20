# Required cmake version
cmake_minimum_required(VERSION 3.15)

# CMP0091: allow setting MSVC runtime library via
# CMAKE_MSVC_RUNTIME_LIBRARY instead of mutating CMAKE_<LANG>_FLAGS.
# Required to match static vcpkg triplets (which build Boost with /MT).
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

# Put the include dirs which are in the source or build tree
# before all other include dirs, so the headers in the sources
# are prefered over the already installed ones
# since cmake 2.4.1
set(CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE ON)

project(pvpgn VERSION 4.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# When a static vcpkg triplet is in use (e.g. x64-windows-static),
# Boost libraries are built with /MT. Match the project to avoid
# RuntimeLibrary mismatches at link time.
if(MSVC AND DEFINED VCPKG_TARGET_TRIPLET AND VCPKG_TARGET_TRIPLET MATCHES "-static$")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    message(STATUS "pvpgn: static vcpkg triplet detected (${VCPKG_TARGET_TRIPLET}); forcing /MT runtime")
endif()

option(WITH_BNETD "compile the bnetd target" ON)
option(WITH_D2CS "compile the d2cs target" ON)
option(WITH_D2DBS "compile the d2dbs target" ON)
option(WITH_LUA "enable Lua support" OFF)
if(WIN32)
	option(WITH_WIN32_GUI "enable GUI building (default on)" ON)
endif(WIN32)

#storage backends flags
option(WITH_MYSQL "include MySQL user accounts support" OFF)
option(WITH_SQLITE3 "include SQLite3 user accounts support" OFF)
option(WITH_PGSQL "include PostgreSQL user accounts support" OFF)
option(WITH_ODBC "include ODBC user accounts support" OFF)

# ---- v3 refactor opt-in -------------------------------------------------
# When PVPGN_BUILD_V3=ON, the new C++20 sub-tree under src/v3/ is built
# alongside (or instead of) the legacy daemons. Legacy build is untouched
# when PVPGN_BUILD_V3=OFF (the default).
option(PVPGN_BUILD_V3        "Build the refactored v3 sub-tree (experimental)" OFF)
option(PVPGN_BUILD_LEGACY    "Build the legacy bnetd/d2cs/d2dbs tree"          ON)
option(PVPGN_V3_BUILD_TESTS  "Build unit tests for the v3 sub-tree"            ${PVPGN_BUILD_V3})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules")

if(PVPGN_BUILD_LEGACY)
    include(ConfigureChecks.cmake)
endif()


if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	# using Clang

	set( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wno-idiomatic-parentheses -pedantic")

elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	# using G++

	if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.1)
		message(FATAL_ERROR "G++ 5.1 or higher required")
	endif()

	set( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -pedantic -Wno-variadic-macros" )

elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	# using Visual Studio

	if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.0)
		message(FATAL_ERROR "Visual Studio 2015 or higher required")
	endif()

	# UNICODE flags belong to the legacy daemons only — the v3 sub-tree
	# (and its Catch2 test harness) expects narrow `main()` entry points.
	if(PVPGN_BUILD_LEGACY)
		add_definitions(
			-D_CRT_SECURE_NO_DEPRECATE
			-D_CRT_NONSTDC_NO_DEPRECATE
			-DUNICODE
			-D_UNICODE
		)
	else()
		add_definitions(
			-D_CRT_SECURE_NO_DEPRECATE
			-D_CRT_NONSTDC_NO_DEPRECATE
		)
	endif()
	
	# DEBUG compiler flags:
	#	/Zi create debugging information PDB file
	#	/Od disable optimizations
	# 	/Oy- do not suppress frame pointers (recommended for debugging)
	#	/MTd use statically linked, thread-safe, debug CRT libs (Magic Builder set this flag when build)
	#
	# RELEASE compiler flags:
	#	/MT use statically linked, thread-safe CRT libs (Magic Builder set this flag when build)
	# 	/GS- no Buffer Security Check
	#
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Od /Oy-")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")

	# Explaining of linker flags and why enable pdb with debug info for Release build is on:
	#  https://www.wintellect.com/correctly-creating-native-c-release-build-pdbs
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
endif()

if(PVPGN_BUILD_LEGACY)
    add_subdirectory(conf)
    add_subdirectory(files)
    # Prefer system / vcpkg `fmt` (modern, 10.x+). When no package is
    # found we fall back to FetchContent-ing the upstream fmtlib so a
    # checkout without a vcpkg toolchain still builds (network
    # required on first configure).
    find_package(fmt CONFIG QUIET)
    if(fmt_FOUND)
        message(STATUS "Using external fmt ${fmt_VERSION} (vcpkg/system)")
        # Consumers do `target_link_libraries(... fmt)`. Modern fmt
        # exports `fmt::fmt`; alias bare `fmt` to it so consumer
        # sites stay identical regardless of how fmt was obtained.
        if(NOT TARGET fmt)
            add_library(fmt INTERFACE IMPORTED GLOBAL)
            target_link_libraries(fmt INTERFACE fmt::fmt)
        endif()
    else()
        message(STATUS "External fmt not found; fetching fmtlib/fmt via FetchContent")
        include(FetchContent)
        FetchContent_Declare(
            fmt
            GIT_REPOSITORY https://github.com/fmtlib/fmt.git
            GIT_TAG        11.2.0
            GIT_SHALLOW    TRUE
        )
        # fmt's tests / docs aren't needed for our consumers.
        set(FMT_TEST     OFF CACHE BOOL "" FORCE)
        set(FMT_DOC      OFF CACHE BOOL "" FORCE)
        set(FMT_INSTALL  OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(fmt)
        # FetchContent'd fmt already exposes the bare `fmt` target,
        # so no alias is needed.
    endif()

    # Prefer system / vcpkg `nlohmann_json`. The previous in-tree
    # `src/json/json.hpp` snapshot (v3.1.2, 2018) has been retired in
    # favour of the upstream package; legacy consumers
    # (`adbanner.cpp`, `versioncheck.cpp`) now `#include
    # <nlohmann/json.hpp>`. Fall back to FetchContent so a checkout
    # without a vcpkg toolchain still builds.
    find_package(nlohmann_json CONFIG QUIET)
    if(nlohmann_json_FOUND)
        message(STATUS "Using external nlohmann_json ${nlohmann_json_VERSION} (vcpkg/system)")
    else()
        message(STATUS "External nlohmann_json not found; fetching nlohmann/json via FetchContent")
        include(FetchContent)
        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG        v3.11.3
            GIT_SHALLOW    TRUE
        )
        set(JSON_BuildTests OFF CACHE INTERNAL "")
        set(JSON_Install    OFF CACHE INTERNAL "")
        FetchContent_MakeAvailable(nlohmann_json)
    endif()
    add_subdirectory(man)
    add_subdirectory(src)
    if(WITH_LUA)
        add_subdirectory(lua)
    endif(WITH_LUA)
endif()

enable_testing()

if(PVPGN_BUILD_V3)
    # The v3 sub-tree (and its Catch2 test harness) expects a narrow
    # `main()`. When PVPGN_BUILD_LEGACY=ON we added `-DUNICODE
    # -D_UNICODE` globally for the legacy daemons, which would force
    # MSVC's CRT to expect `wmain` in every executable, breaking the
    # v3 test targets. Strip those flags before descending into the
    # v3 subdirectory so v3 targets see narrow `main()`.
    if (MSVC)
        remove_definitions(-DUNICODE -D_UNICODE)
    endif()
    add_subdirectory(src/v3)
    add_subdirectory(tests)
endif()

# uninstall target
if(PVPGN_BUILD_LEGACY)
configure_file(
    "${CMAKE_MODULE_PATH}/cmake_uninstall.cmake.in"
    "${CMAKE_MODULE_PATH}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_MODULE_PATH}/cmake_uninstall.cmake)
	
# purge target
configure_file(
    "${CMAKE_MODULE_PATH}/cmake_purge.cmake.in"
    "${CMAKE_MODULE_PATH}/cmake_purge.cmake"
    IMMEDIATE @ONLY)

add_custom_target(purge
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_MODULE_PATH}/cmake_purge.cmake)
endif()
