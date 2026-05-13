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

    include(Catch)
    catch_discover_tests(${name})
endfunction()
