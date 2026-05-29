# SPDX-License-Identifier: GPL-2.0-or-later
#
# NLS/SRP-6a crypto library for Warcraft III / W3XP authentication.
# Depends on OpenSSL::Crypto for BN_* big-number operations and SHA-1.

pvpgn_v3_add_library(infra_crypto_nls
    SOURCES
        src/nls.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
        OpenSSL::Crypto
)
