## d2dbs: split into a static library + thin executable.
##
## Mirrors the bnetd / d2cs carve-out: the bulk of the d2dbs
## code lives in d2dbs_legacy so v3 strangler-fig bridges can
## call into legacy headers without re-linking the executable.

set(D2DBS_LIB_SOURCES
	charlock.cpp charlock.h cmdline.cpp cmdline.h d2ladder.cpp d2ladder.h 
	dbsdupecheck.cpp dbsdupecheck.h dbserver.cpp dbserver.h dbspacket.cpp 
	dbspacket.h handle_signal.cpp handle_signal.h 
	setup.h version.h)

set(D2DBS_EXE_SOURCES
	main.cpp
	../win32/d2dbs_winmain.cpp ../win32/d2dbs_resource.h
	../win32/d2dbs_resource.rc)

# R165: legacy `prefs.cpp` / `prefs.h` deleted; v3 TOML loader
# (`pvpgn_v3_d2dbs_prefs_load_toml`) is the sole config source.

add_library(d2dbs_legacy STATIC ${D2DBS_LIB_SOURCES})
target_include_directories(d2dbs_legacy
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(d2dbs_legacy
    PUBLIC
        common compat fmt win32 ${NETWORK_LIBRARIES})

if(WITH_WIN32_GUI)
  add_executable(d2dbs WIN32 ${D2DBS_EXE_SOURCES})
else(WITH_WIN32_GUI)
  add_executable(d2dbs ${D2DBS_EXE_SOURCES})
endif(WITH_WIN32_GUI)

target_link_libraries(d2dbs PRIVATE d2dbs_legacy)

# v3 strangler-fig wiring MOVED to
# src/v3/integration/legacy_d2dbs/CMakeLists.txt (added LATE by the
# top-level configure -- after `add_subdirectory(src)` -- so that
# d2dbs_legacy actually exists when the wiring runs). Mirrors the
# R196.b / R199 fix for d2cs / d2dbs. See plans/r199-checklist.md.

install(TARGETS d2dbs DESTINATION ${SBINDIR})
if(WIN32 AND MSVC)
    install(FILES $<TARGET_PDB_FILE:d2dbs> DESTINATION ${SBINDIR} OPTIONAL)
endif()
