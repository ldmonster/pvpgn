# SPDX-License-Identifier: GPL-2.0-or-later

pvpgn_v3_add_library(domain_ladder
    SOURCES
        src/d2_ladder.cpp
    HEADERS
        include/domain/ladder/ladder.hpp
        include/domain/ladder/d2_ladder.hpp
    DEPS
        core
        domain_realm
)
