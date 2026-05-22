/*
 * server_v3_hook.cpp
 *
 * Implementation of server_tick_v3().
 *
 * When PVPGN_V3_BNETD_INTEGRATION is defined, this translation unit
 * includes the v3 LegacyBridge header (which requires C++20 and Boost.Asio)
 * and delegates to LegacyBridge::instance().tick().
 *
 * When PVPGN_V3_BNETD_INTEGRATION is NOT defined, server_tick_v3() is a
 * no-op that compiles with any C++11-compatible compiler.
 */

#include "server_v3_hook.h"

#ifdef PVPGN_V3_BNETD_INTEGRATION

// The LegacyBridge header and its Asio dependencies are only pulled in
// when the integration macro is active.  This keeps the legacy build
// free of any Boost dependency.
#include "app/bnetd/legacy_bridge.hpp"

#include <chrono>

namespace pvpgn
{
    namespace bnetd
    {

        void server_tick_v3(unsigned int budget_ms)
        {
            pvpgn::app::bnetd::LegacyBridge::instance().tick(
                std::chrono::milliseconds{budget_ms});
        }

    }  // namespace bnetd
}  // namespace pvpgn

#else  /* !PVPGN_V3_BNETD_INTEGRATION */

namespace pvpgn
{
    namespace bnetd
    {

        void server_tick_v3(unsigned int /*budget_ms*/)
        {
            /* no-op: v3 integration not enabled */
        }

    }  // namespace bnetd
}  // namespace pvpgn

#endif  /* PVPGN_V3_BNETD_INTEGRATION */
