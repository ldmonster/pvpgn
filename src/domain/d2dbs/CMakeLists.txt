# SPDX-License-Identifier: GPL-2.0-or-later
# ---- domain/d2dbs (D2DBS domain layer) -------------------------------------
#
# Header-only interface library.  All types, repository interfaces, use cases,
# and in-memory test implementations live in headers under include/.
#
# Consumers link against `domain_d2dbs` to get the include path; no .cpp
# compilation units are needed.

pvpgn_v3_add_library(domain_d2dbs
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
)
