# SPDX-License-Identifier: GPL-2.0-or-later
# src/v3/app/pvpgn-config/CMakeLists.txt
#
# Defines the `pvpgn_config_tool` executable — a standalone CLI tool for
# inspecting and validating PvPGN server configuration files.
#
# Supported commands
# ------------------
#   --validate <file>
#       Load the config file, report any errors; exit 0 on success / 1 on error.
#
#   --print-effective <file>
#       Load the config file + apply env-var overrides, print the effective
#       config as TOML (with secrets redacted as "***").
#
#   --print-schema
#       Print a Markdown table of all config keys with their types and defaults.
#
#   --help, -h
#       Print usage.
#
# Dependencies
# ------------
#   pvpgn_infra_config  — load_server_config, ServerConfig, Secret<T>
#   pvpgn_core          — core::Result, core::Error

cmake_minimum_required(VERSION 3.20)

add_executable(pvpgn_config_tool
    main.cpp
)

# Apply v3 warning flags + C++20 standard
pvpgn_v3_apply_flags(pvpgn_config_tool)

target_link_libraries(pvpgn_config_tool
    PRIVATE
        pvpgn_infra_config
        pvpgn_core
)

install(TARGETS pvpgn_config_tool
    RUNTIME DESTINATION ${BIN_INSTALL_DIR}
)
