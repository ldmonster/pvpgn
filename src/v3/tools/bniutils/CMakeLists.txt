# PvPGN v3 bniutils tools -- BNI image / TGA conversion CLI suite.
#
# Modernized port of legacy `src/bniutils/`:
#   * No link against legacy `common` / `compat`.
#   * Allocation routed through a local `xalloc_shim.hpp` that wraps
#     `std::malloc` / `std::realloc` / `std::free` with abort-on-OOM
#     (matches the long-standing behavior of `common/xalloc`).
#   * Directory checks use `std::filesystem` instead of POSIX
#     `stat()` / `mkdir()` / `compat/statmacros.h` / `compat/mkdir.h`.
#   * `PVPGN_VERSION` is supplied as a compile definition (falling
#     back to "unknown" when undefined).
#
# All five executables keep their legacy names so packaging,
# scripts, and docs continue to work unchanged.

set(_bniutils_dir ${CMAKE_CURRENT_SOURCE_DIR})

function(_pvpgn_v3_add_bniutils_tool name)
    add_executable(${name} ${ARGN})
    pvpgn_v3_apply_flags(${name})
    target_include_directories(${name} PRIVATE ${_bniutils_dir})
    if(DEFINED PVPGN_VERSION AND NOT PVPGN_VERSION STREQUAL "")
        target_compile_definitions(${name} PRIVATE
            PVPGN_VERSION="${PVPGN_VERSION}")
    endif()
    install(TARGETS ${name} RUNTIME DESTINATION ${BINDIR})
endfunction()

_pvpgn_v3_add_bniutils_tool(bnilist
    bnilist.cpp fileio.cpp fileio.h tga.cpp tga.h)

_pvpgn_v3_add_bniutils_tool(bni2tga
    bni2tga.cpp fileio.cpp fileio.h)

_pvpgn_v3_add_bniutils_tool(bniextract
    bniextract.cpp fileio.cpp fileio.h tga.cpp tga.h bni.cpp bni.h)

_pvpgn_v3_add_bniutils_tool(bnibuild
    bnibuild.cpp fileio.cpp fileio.h bni.cpp bni.h tga.cpp tga.h)

_pvpgn_v3_add_bniutils_tool(tgainfo
    tgainfo.cpp fileio.cpp fileio.h tga.cpp tga.h)
