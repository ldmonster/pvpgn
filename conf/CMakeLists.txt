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

# The v3 servers (bnetd / d2cs / d2dbs) are always built, so always ship their
# starter configs. The old WITH_BNETD / WITH_D2CS / SYSCONFDIR gating referred
# to legacy build variables that no longer exist post-v3-migration, which left
# `make install` shipping no configs at all (so the runtime image's
# /etc/pvpgn was never created). Install to the canonical SYSCONF_INSTALL_DIR
# (set by the build / Dockerfile), falling back to the legacy SYSCONFDIR.
set(_pvpgn_sysconfdir "${SYSCONF_INSTALL_DIR}")
if(NOT _pvpgn_sysconfdir)
  set(_pvpgn_sysconfdir "${SYSCONFDIR}")
endif()

set(PVPGN_STARTER_CONFS
  ad.json autoupdate.conf bnban.conf bnetd_default_user.plain
  bnissue.txt channel.conf versioncheck.json
  bnetd.toml d2cs.toml d2dbs.toml)

if(_pvpgn_sysconfdir)
  foreach(CONF_FILE ${PVPGN_STARTER_CONFS})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CONF_FILE}
            DESTINATION ${_pvpgn_sysconfdir})
  endforeach()
endif()

add_subdirectory(i18n)
