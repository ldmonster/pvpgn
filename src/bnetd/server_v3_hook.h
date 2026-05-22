/*
 * server_v3_hook.h
 *
 * Hook for calling the v3 Asio event loop from the legacy bnetd server loop.
 *
 * When `PVPGN_V3_BNETD_INTEGRATION` is defined (i.e. the v3 tree is present
 * and the integration bridge has been initialised), `server_tick_v3()` calls
 * `LegacyBridge::instance().tick(budget_ms)` to let the v3 Asio io_context
 * make progress for up to `budget_ms` milliseconds.
 *
 * When `PVPGN_V3_BNETD_INTEGRATION` is NOT defined (legacy-only build),
 * `server_tick_v3()` is a no-op and compiles away entirely.
 *
 * Header compatibility
 * --------------------
 * This header is valid C++11 and may be included from any legacy C++ file.
 * It does NOT expose any C++20 features or v3 types.
 */

#ifndef INCLUDED_SERVER_V3_HOOK_H
#define INCLUDED_SERVER_V3_HOOK_H

#ifdef __cplusplus

namespace pvpgn
{
    namespace bnetd
    {

        /**
         * @brief Tick the v3 Asio event loop.
         *
         * Call this once per iteration of the legacy server main loop.
         * The v3 Asio io_context will be allowed to run for at most
         * `budget_ms` milliseconds before this function returns.
         *
         * @param budget_ms  Maximum time (in milliseconds) to spend in the
         *                   v3 event loop.  A value of 0 means "poll only —
         *                   return immediately if there is no pending work".
         */
        void server_tick_v3(unsigned int budget_ms);

    }  // namespace bnetd
}  // namespace pvpgn

#endif  /* __cplusplus */

#endif  /* INCLUDED_SERVER_V3_HOOK_H */
