# PvPGN 4.0 Configuration Converter Tool
# Converts legacy bnetd.conf (INI format) to TOML format

add_executable(pvpgn-conf-convert
    main.cpp
)

pvpgn_v3_apply_flags(pvpgn-conf-convert)

# No external dependencies required for this tool
# It uses only standard C++ library

install(TARGETS pvpgn-conf-convert
    RUNTIME DESTINATION ${BIN_INSTALL_DIR}
)
