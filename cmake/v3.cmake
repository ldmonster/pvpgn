# cmake/v3.cmake — helper module for the new (v3) build sub-tree.
#
# Provides:
#   pvpgn_v3_add_library(<name> [INTERFACE] [STATIC]
#                        SOURCES ...
#                        PUBLIC_INCLUDES ...
#                        DEPS ...)
#   pvpgn_v3_add_test(<name> SOURCES ... DEPS ...)
#   pvpgn_v3_apply_flags(<target>)         — warnings + std + flags
#
# Conventions:
#   * C++20 only.
#   * Public headers in <module>/include/, private in <module>/src/.
#   * All targets get warnings-as-errors in Debug.

include_guard(GLOBAL)

set(PVPGN_V3_CXX_STANDARD 20 CACHE STRING "C++ standard for the v3 sub-tree")

option(PVPGN_V3_WARNINGS_AS_ERRORS "Treat warnings as errors in v3 targets"
       ON)

set(PVPGN_V3_SANITIZERS "" CACHE STRING
    "Semicolon list of sanitizers for v3 targets (e.g. address;undefined)")

option(PVPGN_V3_COVERAGE "Enable coverage instrumentation on v3 targets" OFF)


function(pvpgn_v3_apply_flags target)
    target_compile_features(${target} PUBLIC cxx_std_${PVPGN_V3_CXX_STANDARD})
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD          ${PVPGN_V3_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS        OFF)

    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4 /permissive- /Zc:__cplusplus /Zc:preprocessor /utf-8 /EHsc
            # R214: a discarded [[nodiscard]] return value is a hard build
            # error on all v3 targets (Result<T,E>, Status<T>, observers).
            /we4834
            $<$<CONFIG:Debug>:/Od /Zi>)
        if(PVPGN_V3_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic
            -Wshadow -Wnon-virtual-dtor
            -Wold-style-cast -Wcast-align
            -Woverloaded-virtual -Wnull-dereference
            -Wdouble-promotion -Wformat=2
            -Wno-unused-parameter
            # R214: a discarded [[nodiscard]] return value is a hard build
            # error on all v3 targets (Result<T,E>, Status<T>, observers).
            -Werror=unused-result
            # GCC's -Wmaybe-uninitialized has well-known false positives
            # around std::variant/optional with non-trivial alternatives.
            -Wno-maybe-uninitialized)
        if(PVPGN_V3_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()

    if(PVPGN_V3_SANITIZERS)
        foreach(_san IN LISTS PVPGN_V3_SANITIZERS)
            target_compile_options(${target} PRIVATE -fsanitize=${_san})
            target_link_options(${target} PRIVATE    -fsanitize=${_san})
        endforeach()
        target_compile_options(${target} PRIVATE -fno-omit-frame-pointer)
    endif()

    if(PVPGN_V3_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE --coverage -O0 -g)
        target_link_options(${target}    PRIVATE --coverage)
    endif()
endfunction()


function(pvpgn_v3_add_library name)
    set(opts INTERFACE STATIC)
    set(svals)
    set(mvals SOURCES PUBLIC_INCLUDES PRIVATE_INCLUDES DEPS PUBLIC_DEPS)
    cmake_parse_arguments(P "${opts}" "${svals}" "${mvals}" ${ARGN})

    if(P_INTERFACE)
        add_library(${name} INTERFACE)
        foreach(_d IN LISTS P_PUBLIC_INCLUDES)
            target_include_directories(${name} INTERFACE
                $<BUILD_INTERFACE:${_d}>)
        endforeach()
        if(P_DEPS OR P_PUBLIC_DEPS)
            target_link_libraries(${name} INTERFACE ${P_PUBLIC_DEPS} ${P_DEPS})
        endif()
    else()
        add_library(${name} STATIC ${P_SOURCES})
        pvpgn_v3_apply_flags(${name})
        foreach(_d IN LISTS P_PUBLIC_INCLUDES)
            target_include_directories(${name} PUBLIC
                $<BUILD_INTERFACE:${_d}>)
        endforeach()
        foreach(_d IN LISTS P_PRIVATE_INCLUDES)
            target_include_directories(${name} PRIVATE ${_d})
        endforeach()
        if(P_PUBLIC_DEPS)
            target_link_libraries(${name} PUBLIC ${P_PUBLIC_DEPS})
        endif()
        if(P_DEPS)
            target_link_libraries(${name} PRIVATE ${P_DEPS})
        endif()
    endif()

    add_library(pvpgn::v3::${name} ALIAS ${name})
endfunction()


function(pvpgn_v3_add_test name)
    set(svals)
    set(mvals SOURCES DEPS)
    cmake_parse_arguments(P "" "${svals}" "${mvals}" ${ARGN})

    add_executable(${name} ${P_SOURCES})
    pvpgn_v3_apply_flags(${name})
    target_link_libraries(${name} PRIVATE Catch2::Catch2WithMain ${P_DEPS})

    # Disable non-virtual-dtor warning for tests since Catch2 has this issue
    if(NOT MSVC)
        target_compile_options(${name} PRIVATE -Wno-non-virtual-dtor)
    endif()

    include(Catch)
    catch_discover_tests(${name})
endfunction()


# pvpgn_v3_add_header_selfcheck(<name>
#     INCLUDE_ROOT <abs_path>
#     HEADERS h1 [h2 ...]
#     [DEPS lib1 lib2 ...])
#
# R213 (plans/01-modern-cpp-baseline.md S3): per header, generate a
# tiny .cpp that does nothing but `#include` the header. Compile all
# generated TUs into one Catch2 test executable that contains a single
# trivially-passing test case. If any header is not self-contained
# (i.e. it forgets to #include one of its own dependencies, or relies
# on transitive includes), the compile fails -- which is the whole
# point.
#
# HEADERS entries are paths relative to INCLUDE_ROOT.
function(pvpgn_v3_add_header_selfcheck name)
    set(svals INCLUDE_ROOT)
    set(mvals HEADERS DEPS)
    cmake_parse_arguments(P "" "${svals}" "${mvals}" ${ARGN})

    if(NOT P_INCLUDE_ROOT)
        message(FATAL_ERROR "pvpgn_v3_add_header_selfcheck(${name}): INCLUDE_ROOT required")
    endif()
    if(NOT P_HEADERS)
        message(FATAL_ERROR "pvpgn_v3_add_header_selfcheck(${name}): HEADERS required")
    endif()

    set(_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/${name}_gen")
    file(MAKE_DIRECTORY "${_gen_dir}")

    set(_gen_sources)
    foreach(_hdr IN LISTS P_HEADERS)
        # Build a slug for the generated filename.
        string(REPLACE "/" "_" _slug "${_hdr}")
        string(REPLACE "." "_" _slug "${_slug}")
        set(_out "${_gen_dir}/include_${_slug}.cpp")
        file(WRITE "${_out}"
"// AUTOGENERATED by pvpgn_v3_add_header_selfcheck (R213).\n"
"// Verifies that ${_hdr} is self-contained.\n"
"#include \"${_hdr}\"\n"
"namespace { [[maybe_unused]] int _pvpgn_selfcheck_${_slug} = 0; }\n"
        )
        list(APPEND _gen_sources "${_out}")
    endforeach()

    # One Catch2 entry point so the test executable has a TEST_CASE.
    set(_entry "${_gen_dir}/_selfcheck_main.cpp")
    file(WRITE "${_entry}"
"// AUTOGENERATED by pvpgn_v3_add_header_selfcheck (R213).\n"
"#include <catch2/catch_test_macros.hpp>\n"
"TEST_CASE(\"${name}: headers compile self-contained\", \"[selfcheck]\") {\n"
"    SUCCEED(\"headers compiled in isolation\");\n"
"}\n"
    )

    add_executable(${name} ${_gen_sources} ${_entry})
    pvpgn_v3_apply_flags(${name})
    target_include_directories(${name} PRIVATE "${P_INCLUDE_ROOT}")
    target_link_libraries(${name} PRIVATE Catch2::Catch2WithMain)
    if(P_DEPS)
        target_link_libraries(${name} PRIVATE ${P_DEPS})
    endif()
    if(NOT MSVC)
        target_compile_options(${name} PRIVATE -Wno-non-virtual-dtor)
    endif()

    include(Catch)
    catch_discover_tests(${name})
endfunction()