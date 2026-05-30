# PvPGN v3 bniutils tools -- BNI image / TGA conversion CLI suite.
#
# Modernized port of legacy `src/bniutils/`:
#   * No link against legacy `common` / `compat`. The only shared
#     dependency is the v3 `core` library, used for
#     `core::read_le` / `core::write_le` etc. in `fileio.cpp`.
#   * RAII storage: `t_tgaimg::data` and `t_bnifile::icons` are
#     `std::vector` instances; allocation failures throw
#     `std::bad_alloc` instead of routing through a custom
#     abort-on-OOM shim.
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
    # These legacy image-manipulation tools mix int/size_t/uint*_t
    # liberally on every index expression. A surgical fix would touch
    # ~80 call sites without semantic benefit, so the strict sign /
    # narrowing warnings are downgraded from errors for this target.
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${name} PRIVATE
            -Wno-error=sign-conversion
            -Wno-error=conversion)
    endif()
    target_include_directories(${name} PRIVATE ${_bniutils_dir})
    target_link_libraries(${name} PRIVATE core)
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
