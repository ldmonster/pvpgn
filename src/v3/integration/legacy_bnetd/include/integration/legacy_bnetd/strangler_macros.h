// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef PVPGN_V3_INTEGRATION_LEGACY_BNETD_STRANGLER_MACROS_H
#define PVPGN_V3_INTEGRATION_LEGACY_BNETD_STRANGLER_MACROS_H

/* @file strangler_macros.h
 *
 * One-line strangler dispatch macros for the v3 bridges.
 *
 * Each `pvpgn_v3_<op>_try` C-linkage entry point in
 * `src/v3/integration/legacy_bnetd/src/*_bridge.cpp` is invoked from
 * the matching legacy handler. The call site has historically been a
 * 4-line `#ifdef PVPGN_V3_BNETD_INTEGRATION ... extern "C" int
 * pvpgn_v3_<op>_try(...); if (...try(...)) return 0; #endif` block.
 * `PVPGN_V3_BRIDGE_TRY` collapses that to a single line at the call
 * site, removes the duplicated forward-declaration, and gives the
 * strangler infra one place to evolve (logging, metrics, fault
 * injection) without touching every legacy handler.
 *
 * Usage (from a legacy `.cpp` file):
 *
 *     PVPGN_V3_BRIDGE_TRY(profile, c, packet_get_data_const(p, 4, sz - 4), sz - 4);
 *
 * If the v3 bridge returns >0 (handled), the macro `return`s from the
 * surrounding function with the equivalent of the legacy "I handled
 * this" exit path (which is also just `return 0` in handle_anongame.cpp,
 * so the macro returns void/expression-zero). If it returns 0
 * (fall-through), execution continues with the legacy code path.
 *
 * When `PVPGN_V3_BNETD_INTEGRATION` is not defined the macro expands
 * to a no-op statement, leaving legacy code unchanged.
 */

#ifdef PVPGN_V3_BNETD_INTEGRATION

/* C++-only: callers are .cpp files that already use `extern "C"` for
 * the symbol. The forward declaration below is repeated every
 * macro-call but the linker dedups them. */
#define PVPGN_V3_BRIDGE_TRY(name, conn, body, body_size)                       \
    do {                                                                        \
        extern "C" int pvpgn_v3_##name##_try(                                  \
            void* conn_ptr, void const* body_ptr, unsigned int body_sz);       \
        if (pvpgn_v3_##name##_try((conn), (body), (body_size)) > 0) {          \
            return 0;                                                          \
        }                                                                      \
    } while (0)

#else  /* !PVPGN_V3_BNETD_INTEGRATION */

#define PVPGN_V3_BRIDGE_TRY(name, conn, body, body_size) ((void)0)

#endif /* PVPGN_V3_BNETD_INTEGRATION */

#endif /* PVPGN_V3_INTEGRATION_LEGACY_BNETD_STRANGLER_MACROS_H */
