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

# v3 integration is mandatory as of vN.0 — PVPGN_V3_BNETD_INTEGRATION removed
project(pvpgn VERSION 3.0.0 LANGUAGES C CXX)
# R210 (plans/01-modern-cpp-baseline.md): raised global floor from C++11
# to C++20. The v3 sub-tree already targeted C++20 per-target via
# PVPGN_V3_CXX_STANDARD in cmake/v3.cmake; this bump aligns the legacy
# tree with the same standard so the whole project speaks one dialect.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# When a static vcpkg triplet is in use (e.g. x64-windows-static),
# Boost libraries are built with /MT. Match the project to avoid
# RuntimeLibrary mismatches at link time.
if(MSVC AND DEFINED VCPKG_TARGET_TRIPLET AND VCPKG_TARGET_TRIPLET MATCHES "-static$")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    message(STATUS "pvpgn: static vcpkg triplet detected (${VCPKG_TARGET_TRIPLET}); forcing /MT runtime")
endif()

# ---- v3 build options ---------------------------------------------------
option(PVPGN_V3_BUILD_TESTS  "Build unit tests for the v3 sub-tree"  ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules")


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

	add_definitions(
		-D_CRT_SECURE_NO_DEPRECATE
		-D_CRT_NONSTDC_NO_DEPRECATE
	)
	
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

add_subdirectory(src)

enable_testing()

add_subdirectory(tests)
