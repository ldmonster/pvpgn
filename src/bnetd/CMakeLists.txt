## bnetd: split into a static library + thin executable.
##
## Rationale: the v3 strangler-fig adapter
## (`src/v3/integration/legacy_bnetd/`) needs to call the legacy
## `handle_*_packet` functions without re-linking the entire bnetd
## executable. Exposing the bulk of the code as a STATIC library
## lets v3 link selectively while the original `bnetd` binary keeps
## building exactly as before.

set(BNETD_LIB_SOURCES
	account.cpp account.h account_wrap.cpp account_wrap.h adbanner.cpp
	adbanner.h alias_command.cpp alias_command.h anongame.cpp
	anongame_gameresult.cpp anongame_gameresult.h anongame.h
	anongame_infos.cpp anongame_infos.h anongame_maplists.cpp
	anongame_maplists.h attrgroup.cpp attrgroup.h attr.h attrlayer.cpp
	attrlayer.h autoupdate.cpp autoupdate.h channel_conv.cpp channel_conv.h
	channel.cpp channel.h character.cpp character.h clan.cpp clan.h
	cmdline.cpp cmdline.h command.cpp command_groups.cpp command_groups.h
	command.h connection.cpp connection.h file.cpp file.h file_plain.cpp
	file_plain.h friends.cpp friends.h game_conv.cpp
	game_conv.h game.cpp game.h handle_anongame.cpp handle_anongame.h
	handle_apireg.cpp handle_apireg.h handle_bnet.cpp handle_bnet.h
	handle_bot.cpp handle_bot.h handle_d2cs.cpp handle_d2cs.h
	handlers.h handle_telnet.cpp handle_telnet.h handle_udp.cpp
	handle_udp.h handle_wol.cpp handle_wol.h helpfile.cpp helpfile.h
	ipban.cpp ipban.h irc.cpp irc.h ladder_calc.cpp ladder_calc.h ladder.cpp
	ladder.h mail.cpp mail.h message.cpp message.h news.cpp news.h
	output.cpp output.h quota.h realm.cpp realm.h
	runprog.cpp runprog.h server.cpp server.h sql_common.cpp sql_common.h
	sql_dbcreator.cpp sql_dbcreator.h sql_mysql.cpp sql_mysql.h sql_odbc.cpp
	sql_odbc.h sql_pgsql.cpp sql_pgsql.h sql_sqlite3.cpp sql_sqlite3.h
	storage.cpp storage_file.cpp storage_file.h storage.h
	storage_sql.cpp storage_sql.h support.cpp support.h
	team.cpp team.h tick.cpp tick.h timer.cpp timer.h topic.cpp topic.h
	tournament.cpp tournament.h icons.cpp icons.h tracker.cpp tracker.h udptest_send.cpp
	udptest_send.h versioncheck.cpp versioncheck.h watch.cpp watch.h
	anongame_wol.cpp anongame_wol.h handle_wserv.cpp handle_wserv.h
	luafunctions.cpp luafunctions.h luainterface.cpp luainterface.h
	luaobjects.cpp luaobjects.h luawrapper.cpp luawrapper.h
	i18n.cpp i18n.h userlog.cpp userlog.h
	server_v3_hook.cpp
	)

## main.cpp + winmain.cpp stay with the executable so the library is
## free of `extern int main(int, char**)` and can be linked from the
## v3 tree (and one day from tests).
set(BNETD_EXE_SOURCES
	main.cpp
	../win32/winmain.cpp ../win32/winmain.h
	)

set(BNETD_CONSOLE_RESOURCES
  ../win32/console_resource.h ../win32/console_resource.rc)

set(BNETD_GUI_RESOURCES
  ../win32/resource.h ../win32/resource.rc)

# ---- bnetd_legacy: the strangler-fig handle ----------------------------
# R165: legacy `prefs.cpp` / `prefs.h` deleted; v3 TOML loader
# (`pvpgn_v3_prefs_load_toml` in `integration_legacy_bnetd_linked`) is
# the sole config source. The `prefs_v3` shim now compiles to a direct
# bridge call unconditionally.
add_library(bnetd_legacy STATIC ${BNETD_LIB_SOURCES})
target_include_directories(bnetd_legacy
    PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}
		${ZLIB_INCLUDE_DIRS}
		$<$<BOOL:${LUA_INCLUDE_DIR}>:${LUA_INCLUDE_DIR}>
		${MYSQL_INCLUDE_DIR}
		${SQLITE3_INCLUDE_DIR}
		${PGSQL_INCLUDE_DIR}
		${ODBC_INCLUDE_DIR}
)
target_link_libraries(bnetd_legacy
    PUBLIC
		common compat fmt nlohmann_json::nlohmann_json win32
		${NETWORK_LIBRARIES} ${ZLIB_LIBRARIES} ${MYSQL_LIBRARIES}
		${SQLITE3_LIBRARIES} ${PGSQL_LIBRARIES} ${ODBC_LIBRARIES}
		$<$<BOOL:${LUA_LIBRARIES}>:${LUA_LIBRARIES}>
)

# When the v3 tree is present, expose infra/compat/include so that
# migrated files can #include "infra/compat/directory.hpp" etc.
# When PVPGN_BUILD_V3=OFF the infra_compat CMake target is never created,
# so fall back to adding the raw include directory directly.
if(TARGET infra_compat)
    target_link_libraries(bnetd_legacy PUBLIC infra_compat)
else()
    target_include_directories(bnetd_legacy PUBLIC
        ${CMAKE_SOURCE_DIR}/src/v3/infra/compat/include)
endif()

# When the v3 XML wrapper is present, link it so that i18n.cpp can
# #include "infra/xml/xml_document.hpp" instead of "common/pugixml.h".
# When PVPGN_BUILD_V3=OFF the infra_xml CMake target is never created,
# so fall back to adding the raw include directory and linking pugixml::static
# directly (pugixml is fetched by the root CMakeLists.txt in that case).
if(TARGET infra_xml)
    target_link_libraries(bnetd_legacy PUBLIC infra_xml)
else()
    target_include_directories(bnetd_legacy PUBLIC
        ${CMAKE_SOURCE_DIR}/src/v3/infra/xml/include)
    if(TARGET pugixml::static)
        target_link_libraries(bnetd_legacy PUBLIC pugixml::static)
    elseif(TARGET pugixml-static)
        target_link_libraries(bnetd_legacy PUBLIC pugixml-static)
    endif()
endif()

# ---- bnetd: the original executable, now a thin wrapper ----------------
if(WITH_WIN32_GUI)
  add_executable(bnetd WIN32 ${BNETD_EXE_SOURCES} ${BNETD_GUI_RESOURCES})
else(WITH_WIN32_GUI)
  add_executable(bnetd ${BNETD_EXE_SOURCES} ${BNETD_CONSOLE_RESOURCES})
endif(WITH_WIN32_GUI)

target_link_libraries(bnetd PRIVATE bnetd_legacy)

# v3 strangler-fig: when the v3 build is in the same configure,
# link the bnetd executable against the linked integration adapter
# so the UDP receive path can be served by `infra::net::UdpEndpoint`.
if(TARGET integration_legacy_bnetd_linked)
    target_link_libraries(bnetd PRIVATE integration_legacy_bnetd_linked)
    if(TARGET infra_log_simple)
        # JsonLineLogger (Batch 23c) is referenced from server.cpp
        # behind PVPGN_V3_BNETD_INTEGRATION; link the lib + expose its
        # public include path on the legacy compile unit.
        target_link_libraries(bnetd PRIVATE infra_log_simple)
        target_include_directories(bnetd_legacy PRIVATE
            ${CMAKE_SOURCE_DIR}/src/v3/infra/log/include)
    endif()
    # Batch 26c: PasswordRotationObserver + InMemoryEventBus are
    # referenced from server.cpp behind PVPGN_V3_AUDIT_LOG.
    if(TARGET application_auth)
        target_link_libraries(bnetd PRIVATE application_auth)
    endif()
    target_compile_definitions(bnetd PRIVATE PVPGN_V3_BNETD_INTEGRATION=1)
    # bnetd_legacy itself hosts the legacy `_client_anongame_*` /
    # `handle_command` etc. handlers that contain the
    # `#ifdef PVPGN_V3_BNETD_INTEGRATION` strangler hooks. Without the
    # define on the lib those hooks were dead code (compiled-out) even
    # when the bridges were linked into the EXE. Enable them here and
    # expose the v3 strangler macro header on the legacy include path.
    target_compile_definitions(bnetd_legacy PRIVATE PVPGN_V3_BNETD_INTEGRATION=1)
    target_include_directories(bnetd_legacy PRIVATE
        ${CMAKE_SOURCE_DIR}/src/v3/integration/legacy_bnetd/include
        ${CMAKE_SOURCE_DIR}/src/v3/application/chat/include
        ${CMAKE_SOURCE_DIR}/src/v3/application/i18n/include
        ${CMAKE_SOURCE_DIR}/src/v3/application/auth/include
        ${CMAKE_SOURCE_DIR}/src/v3/application/ports/include
        ${CMAKE_SOURCE_DIR}/src/v3/domain/shared/include
        ${CMAKE_SOURCE_DIR}/src/v3/domain/identity/include
        ${CMAKE_SOURCE_DIR}/src/v3/infra/inmemory/include
        ${CMAKE_SOURCE_DIR}/src/v3/core/include)
endif()

install(TARGETS bnetd DESTINATION ${SBINDIR})
if(WIN32 AND MSVC)
    install(FILES $<TARGET_PDB_FILE:bnetd> DESTINATION ${SBINDIR} OPTIONAL)
endif()
