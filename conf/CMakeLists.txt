# generate the configs with proper line endings
# Plan 03: removed inlined files (topics.conf, bnalias.conf, bnmaps.conf,
#   bnxplevel.conf, bnxpcalc.conf, command_groups.conf, supportfile.conf,
#   address_translation.conf, tournament.conf, anongame_infos.conf,
#   icons.conf, realm.conf) and deleted sql_DB_layout.conf — all content
#   is now in bnetd.toml.in.
set(OUTPUT_CONFS ad.json autoupdate.conf bnban.conf bnetd_default_user.plain
  bnissue.txt channel.conf versioncheck.json)
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
  set(BNETD_CONFS ad.json autoupdate.conf bnban.conf
  bnetd_default_user.plain bnissue.txt channel.conf
  versioncheck.json bnetd.toml)
endif(WITH_BNETD)

if(WITH_D2CS)
  set(D2CS_CONFS d2cs.toml)
endif(WITH_D2CS)

foreach(CONF_FILE ${BNETD_CONFS} ${D2CS_CONFS})
  if(SYSCONFDIR)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CONF_FILE} DESTINATION ${SYSCONFDIR})
  endif()
endforeach(CONF_FILE)

add_subdirectory(i18n)
