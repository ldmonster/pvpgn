# cmake/v3_warnings.cmake — compiler warning helpers for the v3 sub-tree.
#
# Provides two macros:
#
#   pvpgn_v3_target_warnings(<target>)
#       Applies a comprehensive set of -W flags (GCC/Clang) or /W4 /permissive-
#       (MSVC) to <target> as PRIVATE compile options.
#
#   pvpgn_v3_target_werror(<target>)
#       Adds -Werror (GCC/Clang) or /WX (MSVC) to <target> as PRIVATE compile
#       options.  Call this after pvpgn_v3_target_warnings().
#
# Usage (from pvpgn_v3_add_library / pvpgn_v3_add_test):
#
#   include(v3_warnings)
#   pvpgn_v3_target_warnings(my_target)
#   if(PVPGN_V3_WARNINGS_AS_ERRORS)
#       pvpgn_v3_target_werror(my_target)
#   endif()
#
# CMake coding standards:
#   * Use target_compile_options(... PRIVATE ...) — never add_compile_options().
#   * Generator expressions are used to keep flags out of INTERFACE propagation.

include_guard(GLOBAL)

macro(pvpgn_v3_target_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4           # High warning level
            /permissive-  # Strict conformance mode
        )
    else()
        # GCC and Clang share these flags.
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wconversion
            -Wsign-conversion
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
            -Wimplicit-fallthrough
            # Suppress well-known false positives in GCC with std::variant/optional
            -Wno-maybe-uninitialized
        )
    endif()
endmacro()


macro(pvpgn_v3_target_werror target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /WX)
    else()
        target_compile_options(${target} PRIVATE -Werror)
    endif()
endmacro()
