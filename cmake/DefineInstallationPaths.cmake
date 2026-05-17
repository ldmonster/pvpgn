# Define installation paths for the project
# This module sets up standard installation directories

if(NOT DEFINED BIN_INSTALL_DIR)
    set(BIN_INSTALL_DIR "bin")
endif()

if(NOT DEFINED SBIN_INSTALL_DIR)
    set(SBIN_INSTALL_DIR "sbin")
endif()

if(NOT DEFINED LIB_INSTALL_DIR)
    set(LIB_INSTALL_DIR "lib")
endif()

if(NOT DEFINED INCLUDE_INSTALL_DIR)
    set(INCLUDE_INSTALL_DIR "include")
endif()

if(NOT DEFINED SYSCONF_INSTALL_DIR)
    if(WIN32)
        set(SYSCONF_INSTALL_DIR "etc")
    else()
        set(SYSCONF_INSTALL_DIR "/etc")
    endif()
endif()

if(NOT DEFINED LOCALSTATE_INSTALL_DIR)
    if(WIN32)
        set(LOCALSTATE_INSTALL_DIR "var")
    else()
        set(LOCALSTATE_INSTALL_DIR "/var")
    endif()
endif()

if(NOT DEFINED MAN_INSTALL_DIR)
    if(WIN32)
        set(MAN_INSTALL_DIR "man")
    else()
        set(MAN_INSTALL_DIR "share/man")
    endif()
endif()

if(NOT DEFINED DATA_INSTALL_DIR)
    if(WIN32)
        set(DATA_INSTALL_DIR "share")
    else()
        set(DATA_INSTALL_DIR "share/pvpgn")
    endif()
endif()

if(NOT DEFINED DOC_INSTALL_DIR)
    if(WIN32)
        set(DOC_INSTALL_DIR "doc")
    else()
        set(DOC_INSTALL_DIR "share/doc/pvpgn")
    endif()
endif()
