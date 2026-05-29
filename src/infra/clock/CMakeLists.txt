# SPDX-License-Identifier: GPL-2.0-or-later

pvpgn_v3_add_library(infra_clock
    SOURCES
        src/legacy_clock_bridge.cpp
    HEADERS
        include/infra/clock/legacy_clock_bridge.hpp
    DEPS
        pvpgn_v3_core
)
