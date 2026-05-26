# generate the configs with proper line endings
set(OUTPUT_CONFS ad.json anongame_infos.conf address_translation.conf 
  autoupdate.conf bnalias.conf bnban.conf bnetd_default_user.plain 
  bnissue.txt bnmaps.conf bnxpcalc.conf
  bnxplevel.conf channel.conf command_groups.conf realm.conf 
  sql_DB_layout.conf supportfile.conf topics.conf 
  tournament.conf versioncheck.json icons.conf)
foreach(CONF ${OUTPUT_CONFS})
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${CONF}.in ${CMAKE_CURRENT_BINARY_DIR}/${CONF} @ONLY)
endforeach(CONF)

# generate the config files replacing the path variables
# R166: bnetd.conf.in / d2cs.conf.in / d2dbs.conf.in templates removed
# from the tree -- v3 reads only the .toml siblings. The legacy build
# (PVPGN_BUILD_LEGACY=ON without PVPGN_BUILD_V3=ON) no longer ships
# starter configs; operators are pointed at docs/toml-migration.md for
# upgrades.

# Phase 1 Step 10: TOML configuration templates (v3 ConfigLoader)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/bnetd.toml.in ${CMAKE_CURRENT_BINARY_DIR}/bnetd.toml @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/d2cs.toml.in  ${CMAKE_CURRENT_BINARY_DIR}/d2cs.toml  @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/d2dbs.toml.in ${CMAKE_CURRENT_BINARY_DIR}/d2dbs.toml @ONLY)

if(WITH_BNETD)
  set(BNETD_CONFS ad.json anongame_infos.conf address_translation.conf 
  autoupdate.conf bnalias.conf bnban.conf 
  bnetd_default_user.plain bnissue.txt bnmaps.conf  
  bnxpcalc.conf bnxplevel.conf channel.conf command_groups.conf  
  realm.conf sql_DB_layout.conf supportfile.conf topics.conf 
  tournament.conf versioncheck.json icons.conf bnetd.toml)
endif(WITH_BNETD)

if(WITH_D2CS)
  set(D2CS_CONFS anongame_infos.conf d2cs.toml)
endif(WITH_D2CS)

if(WITH_D2DBS)
  set(D2DBS_CONFS d2dbs.toml)
endif(WITH_D2DBS)

foreach(CONF_FILE ${BNETD_CONFS} ${D2CS_CONFS} ${D2DBS_CONFS})
  if(SYSCONFDIR)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CONF_FILE} DESTINATION ${SYSCONFDIR})
  endif()
endforeach(CONF_FILE)

add_subdirectory(i18n)