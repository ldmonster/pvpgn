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
#
# MSVC warning suppressions (minimum necessary set):
#   /wd4100  — unreferenced formal parameter  (matches GCC -Wno-unused-parameter)
#   /wd4127  — conditional expression is constant  (false positive with if constexpr
#              patterns in older MSVC versions; harmless in C++20 but noisy)
#   /wd4702  — unreachable code  (false positive after [[noreturn]] calls and
#              exhaustive switch/enum patterns; the compiler cannot always prove
#              reachability through inlined [[noreturn]] helpers)

include_guard(GLOBAL)

macro(pvpgn_v3_target_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4           # High warning level
            /permissive-  # Strict conformance mode
            # Minimum necessary suppressions — keep this list short.
            /wd4100       # unreferenced formal parameter (= -Wno-unused-parameter)
            /wd4127       # conditional expression is constant (if constexpr patterns)
            /wd4702       # unreachable code (false positive after [[noreturn]] helpers)
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
        # gcc 15 emits a false-positive `-Wfree-nonheap-object` for
        # `std::vector<std::byte>::push_back` after `reserve()`. Demote
        # to a warning so it doesn't break the build under -Werror.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${target} PRIVATE
                -Wno-error=free-nonheap-object)
            # Under -fsanitize=thread, GCC's -Wtsan fires on libstdc++'s own
            # std::atomic_thread_fence (e.g. inside shared_ptr's atomic refcount
            # release) because ThreadSanitizer cannot model a standalone fence.
            # It is a tooling limitation in the standard library, not a defect in
            # our code and not something we can change, so demote it to a warning
            # rather than failing the tsan build. Inert outside tsan builds.
            target_compile_options(${target} PRIVATE -Wno-error=tsan)
        endif()
    endif()
endmacro()
