# SPDX-License-Identifier: GPL-2.0-or-later
# ---- domain/d2cs (D2CS domain layer — Phase 4 Step 2) ----------------------
#
# Header-only interface library.  All types, repository interfaces, use cases,
# and in-memory test implementations live in headers under include/.
#
# Consumers link against `domain_d2cs` to get the include path; no .cpp
# compilation units are needed.
#
# Phase 4 Step 2 — R145

pvpgn_v3_add_library(domain_d2cs
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
)
