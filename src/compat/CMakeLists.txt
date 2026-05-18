set(COMPAT_SOURCES gethostname.h gettimeofday.cpp gettimeofday.h
	mkdir.h mmap.cpp mmap.h netinet_in.h pdir.cpp pdir.h
	pgetopt.cpp pgetopt.h pgetpid.h psock.cpp psock.h read.h recv.h
	rename.h send.h socket.h
	stdfileno.h strcasecmp.cpp strcasecmp.h
	strerror.cpp strerror.h strncasecmp.cpp strncasecmp.h strsep.cpp
	strsep.h termios.h uname.cpp uname.h)

add_library(compat STATIC ${COMPAT_SOURCES})

# `pdir.cpp` modernized to use `<filesystem>`, which requires C++17.
# The rest of the legacy tree is still C++11 -- only this target is
# bumped, not the global standard.
target_compile_features(compat PUBLIC cxx_std_17)

# GCC < 9 needs `-lstdc++fs` for `std::filesystem`. Modern toolchains
# (GCC >= 9, Clang >= 9 with libc++ >= 7) link it implicitly. We only
# add the flag where strictly required so the rest of the legacy
# tree doesn't pick up an unwanted dependency.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(compat PUBLIC stdc++fs)
endif()

target_link_libraries(compat PRIVATE common fmt)