include_directories(${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR})


if(MYSQL_FOUND)
    add_definitions("-DWITH_SQL_MYSQL")
endif(MYSQL_FOUND)
if(SQLITE3_FOUND)
    add_definitions("-DWITH_SQL_SQLITE3")
endif(SQLITE3_FOUND)
if(PGSQL_FOUND)
    add_definitions("-DWITH_SQL_PGSQL")
endif(PGSQL_FOUND)


if(LUA_FOUND)
    add_definitions("-DWITH_LUA")
endif(LUA_FOUND)



if (WITH_WIN32_GUI)
    add_definitions("-DWIN32_GUI")
endif (WITH_WIN32_GUI)


# Legacy CLI utility dirs that have moved into the v3 tree (still
# built from the same source, just relocated under src/v3/tools/):
#   - `src/bnpass/`   -> `src/v3/tools/bnpass/`   (pvpgn-bnpass)
#   - `src/bniutils/` -> `src/v3/tools/bniutils/` (bnilist, bni2tga,
#                                                  bniextract, bnibuild,
#                                                  tgainfo)
#   - `src/bntrackd/` -> `src/v3/tools/bntrackd/` (bntrackd)
#   - `src/client/`   -> `src/v3/tools/client/`   (bnchat, bnftp,
#                                                  bnbot, bnstat)
#   - `src/bnproxy/`  -> deleted (deprecated since 4.0)
# See plans/refactoring-plan-legacy-tools.md for the migration plan.
add_subdirectory(common)
add_subdirectory(compat)
add_subdirectory(win32)


if(WITH_BNETD)
    add_subdirectory(bnetd)
endif(WITH_BNETD)

if(WITH_D2CS)
    add_subdirectory(d2cs)
endif(WITH_D2CS)

if(WITH_D2DBS)
    add_subdirectory(d2dbs)
endif(WITH_D2DBS)


# `src/test/` removed: the legacy assertion-based bigint and bnetsrp3
# binaries are now Catch2 tests under
# `tests/unit/infra/legacy_crypto/{bigint,bnetsrp3}_test.cpp` and are
# discovered by ctest through the v3 test tree. See
# plans/refactoring-plan-testing.md "Legacy Test Removal".

