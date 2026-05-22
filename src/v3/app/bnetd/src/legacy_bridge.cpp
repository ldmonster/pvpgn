// SPDX-License-Identifier: GPL-2.0-or-later

/// @file legacy_bridge.cpp
/// Implementation of `LegacyBridge`.
///
/// Compiled only when `PVPGN_V3_BNETD_INTEGRATION` is defined.
/// When the macro is absent the header provides inline no-op stubs and
/// this translation unit contributes nothing.

#include "app/bnetd/legacy_bridge.hpp"

#ifdef PVPGN_V3_BNETD_INTEGRATION

#include <memory>
#include <stdexcept>

namespace pvpgn::app::bnetd {

// ---------------------------------------------------------------------------
// Singleton storage
// ---------------------------------------------------------------------------

namespace {
std::unique_ptr<LegacyBridge> g_instance;
}  // namespace

// ---------------------------------------------------------------------------
// Private constructor
// ---------------------------------------------------------------------------

LegacyBridge::LegacyBridge(AsioEventLoop& loop) noexcept
    : loop_{loop}
{}

// ---------------------------------------------------------------------------
// Singleton lifecycle
// ---------------------------------------------------------------------------

LegacyBridge& LegacyBridge::instance() {
    if (!g_instance) {
        throw std::logic_error(
            "LegacyBridge::instance() called before LegacyBridge::init()");
    }
    return *g_instance;
}

void LegacyBridge::init(AsioEventLoop& loop) {
    g_instance = std::unique_ptr<LegacyBridge>(new LegacyBridge(loop));
}

void LegacyBridge::shutdown() {
    g_instance.reset();
}

// ---------------------------------------------------------------------------
// Interleaving API
// ---------------------------------------------------------------------------

void LegacyBridge::tick(std::chrono::milliseconds budget) {
    loop_.run_for(budget);
}

boost::asio::io_context& LegacyBridge::get_io_context() noexcept {
    return loop_.io_context();
}

}  // namespace pvpgn::app::bnetd

#endif  // PVPGN_V3_BNETD_INTEGRATION
