// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ports.hpp
/// Central header for all application ports.
/// Includes all port definitions from domain contexts.
/// Provides backward-compatible using declarations for code that references
/// application::ports::* types (which have been moved to domain contexts).

#include <cstdint>

// Include full definitions for types that need them
#include "domain/identity/ports.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/chat/ports.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/ladder/ports.hpp"
#include "domain/matchmaking/ports.hpp"
#include "domain/realm/ports.hpp"
#include "domain/social/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/shared/ports/config_subscriber.hpp"

// Forward declare connection port (to avoid include path issues)
namespace pvpgn::domain::connection {
class IMessageRouter;
}

namespace pvpgn::application::ports {

// Re-export domain::identity ports
using domain::identity::IAccountRepository;
using domain::identity::IPasswordHasher;
using domain::identity::ISessionRegistry;
using domain::identity::ISessionTokenIssuer;

// Re-export domain::connection ports (forward declared)
using domain::connection::IMessageRouter;

// Re-export domain::moderation ports
using domain::moderation::IPermissionChecker;
using domain::moderation::IAccountBanRepository;
using domain::moderation::IIpBanRepository;
using domain::moderation::Permission;
using domain::moderation::AccountBan;

// Re-export domain::chat ports
using domain::chat::IChannelRepository;

// Re-export domain::gameplay ports
using domain::gameplay::IGameRepository;

// Re-export domain::social ports
using domain::social::IFriendListRepository;
using domain::social::IClanRepository;
using domain::social::ITeamRepository;

// Re-export domain::ladder ports
using domain::ladder::ILadderRepository;

// Re-export domain::matchmaking ports
using domain::matchmaking::IAnonGameCompressor;

// Re-export domain::realm ports
using domain::realm::IRealmRepository;

// Re-export domain::shared ports
using domain::shared::ports::IConfigSubscriber;

// Note: IEventBus is already defined in application::ports namespace
// (see domain/shared/event_bus.hpp), so no re-export needed

}  // namespace pvpgn::application::ports
