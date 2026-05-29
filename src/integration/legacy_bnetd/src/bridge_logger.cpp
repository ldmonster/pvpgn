// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/bridge_logger.hpp"

namespace pvpgn::integration::legacy_bnetd {

namespace {

// Process-global override pointer. Not synchronised; tests are
// single-threaded. nullptr means "fall back to default_logger()".
core::ILogger* g_override = nullptr;

}  // namespace

void set_bridge_logger_override(core::ILogger* override_sink) noexcept {
    g_override = override_sink;
}

core::ILogger& bridge_logger() noexcept {
    if (g_override != nullptr) return *g_override;
    return core::default_logger();
}

}  // namespace pvpgn::integration::legacy_bnetd
