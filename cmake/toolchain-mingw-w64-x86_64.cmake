# =============================================================================
# CMake toolchain file: cross-compile to 64-bit Windows from a Linux host
# using the MinGW-w64 (x86_64-w64-mingw32) GNU toolchain.
#
# Consumed both by the project (via -DCMAKE_TOOLCHAIN_FILE=...) AND by vcpkg
# (via -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=...) so that the project and all
# vcpkg-built dependencies are produced by the SAME compiler/runtime.
# =============================================================================

set(CMAKE_SYSTEM_NAME      Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc-posix)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++-posix)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)
set(CMAKE_AR           ${TOOLCHAIN_PREFIX}-ar      CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB       ${TOOLCHAIN_PREFIX}-ranlib  CACHE FILEPATH "" FORCE)

# Look for libraries/headers ONLY in the mingw sysroot + vcpkg installed tree
set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Fully static .exe (no libgcc / libstdc++ / winpthread DLL dependency)
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-static -static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive")
