## d2cs: split into a static library + thin executable.
##
## Rationale: the v3 strangler-fig adapter
## (`src/v3/integration/legacy_d2cs/`) needs to call the legacy
## d2cs packet/connection code without re-linking the entire d2cs
## executable. Exposing the bulk of the code as a STATIC library
## lets v3 link selectively while the original `d2cs` binary keeps
## building exactly as before. Mirrors the bnetd carve-out.

set(D2CS_LIB_SOURCES
	bit.h bnetd.cpp bnetd.h cmdline.cpp cmdline.h connection.cpp 
	connection.h d2charfile.cpp d2charfile.h d2charlist.cpp d2charlist.h 
	d2gs.cpp d2gs.h d2ladder.cpp d2ladder.h game.cpp game.h gamequeue.cpp 
	gamequeue.h handle_bnetd.cpp handle_bnetd.h handle_d2cs.cpp 
	handle_d2cs.h handle_d2gs.cpp handle_d2gs.h handle_init.cpp 
	handle_init.h handle_signal.cpp handle_signal.h net.cpp 
	net.h s2s.cpp s2s.h server.cpp server.h 
	serverqueue.h serverqueue.cpp setup.h version.h)

## main.cpp + d2cs_winmain.cpp stay with the executable so the
## library is free of `extern int main(int, char**)` and can be
## linked from the v3 tree (and one day from tests).
set(D2CS_EXE_SOURCES
	main.cpp
	../win32/d2cs_winmain.cpp
	../win32/d2cs_resource.h ../win32/d2cs_resource.rc)

# ---- d2cs_legacy: the strangler-fig handle -----------------------------
# R165: legacy `prefs.cpp` / `prefs.h` deleted; v3 TOML loader
# (`pvpgn_v3_d2cs_prefs_load_toml`) is the sole config source.
add_library(d2cs_legacy STATIC ${D2CS_LIB_SOURCES})
target_include_directories(d2cs_legacy
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(d2cs_legacy
    PUBLIC
        common compat fmt win32 ${NETWORK_LIBRARIES})

# When the v3 tree is present, expose infra/compat/include so that
# migrated files can #include "infra/compat/directory.hpp" etc.
if(TARGET infra_compat)
    target_link_libraries(d2cs_legacy PUBLIC infra_compat)
endif()

# ---- d2cs: the original executable, now a thin wrapper -----------------
if(WITH_WIN32_GUI)
  add_executable(d2cs WIN32 ${D2CS_EXE_SOURCES})
else(WITH_WIN32_GUI)
  add_executable(d2cs ${D2CS_EXE_SOURCES})
endif(WITH_WIN32_GUI)

target_link_libraries(d2cs PRIVATE d2cs_legacy)

# v3 strangler-fig: when the v3 build is in the same configure,
# link the d2cs executable against the linked integration adapter
# and define PVPGN_V3_D2CS_INTEGRATION so the legacy #ifdef hooks
# in handle_bnetd.cpp / handle_d2cs.cpp are live.
#
# MOVED to src/v3/integration/legacy_d2cs/CMakeLists.txt by R196.b
# (same R193.fix systemic ordering bug as legacy_bnetd): the previous
# `if(TARGET integration_legacy_d2cs_linked)` guard here was always
# FALSE because `add_subdirectory(src/v3)` runs before
# `add_subdirectory(src)`, so `d2cs_legacy` did not yet exist and the
# linked target was never created. The wiring now lives in a
# CMakeLists added LATE in the top-level configure, after both
# `src/v3` and `src` have been processed. See plans/r196b-checklist.md.

install(TARGETS d2cs DESTINATION ${SBINDIR})
if(WIN32 AND MSVC)
    install(FILES $<TARGET_PDB_FILE:d2cs> DESTINATION ${SBINDIR} OPTIONAL)
endif()
