# copy all files from directory

file(GLOB DEPLOY_FILES_AND_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/*")

foreach(ITEM ${DEPLOY_FILES_AND_DIRS})
   IF( IS_DIRECTORY "${ITEM}" )
      LIST( APPEND DIRS_TO_DEPLOY "${ITEM}" )
   ELSE()
	  IF(NOT ${ITEM} MATCHES "CMakeLists.txt")
		LIST( APPEND FILES_TO_DEPLOY "${ITEM}" )
	  ENDIF(NOT ${ITEM} MATCHES "CMakeLists.txt")
   ENDIF()
endforeach()

# _pvpgn_sysconfdir is resolved in the parent conf/CMakeLists.txt (canonical
# SYSCONF_INSTALL_DIR with a legacy SYSCONFDIR fallback) and inherited here.
if(_pvpgn_sysconfdir)
  INSTALL( FILES ${FILES_TO_DEPLOY} DESTINATION ${_pvpgn_sysconfdir}/i18n )
  INSTALL( DIRECTORY ${DIRS_TO_DEPLOY} DESTINATION ${_pvpgn_sysconfdir}/i18n )
endif()

