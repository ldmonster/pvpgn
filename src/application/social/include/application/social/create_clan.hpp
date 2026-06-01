// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file create_clan.hpp
/// CREATE_CLAN use-case — found a new clan.

#include <memory>
#include <string>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "application/ports/ports.hpp"

namespace pvpgn::application::social {

struct CreateClanRequest {
    domain::AccountId founder_id;
    std::string       tag;    // 2-4 chars
    std::string       name;
};

enum class CreateClanError : std::uint8_t {
    InvalidTag,
    InvalidName,
    TagAlreadyExists,
    PersistenceFailed,
};

class CreateClan {
public:
    CreateClan(std::shared_ptr<application::ports::IClanRepository> clans,
               std::shared_ptr<application::ports::IEventBus> event_bus)
        : clans_(clans), event_bus_(event_bus) {}

    core::Result<domain::ClanId, CreateClanError>
    execute(const CreateClanRequest& req);

private:
    std::shared_ptr<application::ports::IClanRepository> clans_;
    std::shared_ptr<application::ports::IEventBus> event_bus_;
};

}  // namespace pvpgn::application::social
