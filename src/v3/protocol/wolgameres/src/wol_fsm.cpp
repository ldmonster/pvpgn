// SPDX-License-Identifier: GPL-2.0-or-later

#include "protocol/wolgameres/wol_fsm.hpp"

#include <span>

namespace pvpgn::protocol::wol {

core::Status<> WolFsm::on_bytes(std::span<const std::byte> bytes) {
    if (!ctx_) {
        return core::fail(core::make_error(core::StatusCode::Internal,
                                          "session context unavailable"));
    }

    // Stub: Parse WOL game result packet and call handle_game_report
    (void)bytes;
    return core::ok();
}

void WolFsm::on_close() {
    // Cleanup if needed
}

core::Status<> WolFsm::handle_game_report(std::span<const std::byte> packet) {
    // Stub: Parse packet, extract game results, record via repository
    (void)packet;
    return core::ok();
}

}  // namespace pvpgn::protocol::wol
