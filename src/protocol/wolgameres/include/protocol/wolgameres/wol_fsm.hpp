// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wol_fsm.hpp
/// WOL (Westwood Online) game results handler.
/// Parses game result packets and records them.

#include <memory>
#include <span>
#include <cstdint>
#include <cstddef>
#include <cstring>

#include "core/result.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/shared/event_bus.hpp"

// Forward declarations
namespace pvpgn::protocol::bnet {
class ISessionContext;
}
namespace pvpgn::application::ports {
class IGameRepository;
class IEventBus;
}

namespace pvpgn::protocol::wol {

/// WOL game results FSM.
class WolFsm {
public:
    WolFsm(std::shared_ptr<pvpgn::protocol::bnet::ISessionContext> ctx,
            std::shared_ptr<pvpgn::domain::gameplay::IGameRepository> games,
            std::shared_ptr<pvpgn::application::ports::IEventBus> event_bus)
        : ctx_(ctx), games_(games), event_bus_(event_bus) {}

    /// Called when raw bytes are received (WOL game result packet).
    core::Status<> on_bytes(std::span<const std::byte> bytes);

    /// Called when connection is closing.
    void on_close();

private:
    /// Parse WOL packet and record game result.
    core::Status<> handle_game_report(std::span<const std::byte> packet);

    std::shared_ptr<pvpgn::protocol::bnet::ISessionContext> ctx_;
    std::shared_ptr<pvpgn::domain::gameplay::IGameRepository> games_;
    std::shared_ptr<pvpgn::application::ports::IEventBus> event_bus_;
};

}  // namespace pvpgn::protocol::wol
