# SPDX-License-Identifier: GPL-2.0-or-later

pvpgn_v3_add_library(infra_logging
    SOURCES
        src/eventlog_bridge.cpp
    HEADERS
        include/infra/logging/eventlog_bridge.hpp
    DEPS
        pvpgn_v3_core
)
