// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file game_repository.hpp
/// A single, driver-parameterized game repository over `IDbDriver`.

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/gameplay/game.hpp"
#include "domain/gameplay/ports.hpp"
#include "infra/persistence/sql_builder/db_driver.hpp"

namespace pvpgn::infra::persistence {

/// `IGameRepository` over the backend-agnostic `IDbDriver`.
///
/// A game is a parent row plus an ordered player list:
///   games(id PK, host, client_tag, name, map, max_players, state)
///   game_players(game_id, account_id, position)
class SqlGameRepository final : public domain::gameplay::IGameRepository {
public:
    explicit SqlGameRepository(std::shared_ptr<IDbDriver> driver)
        : driver_(std::move(driver)) {}

    [[nodiscard]] core::Result<std::shared_ptr<domain::gameplay::Game>,
                               core::Error>
    find_by_name(std::string_view name) override;

    [[nodiscard]] core::Result<std::shared_ptr<domain::gameplay::Game>,
                               core::Error>
    find_by_id(std::uint32_t id) override;

    core::Result<void, core::Error> save(
        const domain::gameplay::Game& game) override;

    core::Result<void, core::Error> remove(std::string_view name) override;

    [[nodiscard]] core::Result<std::vector<std::shared_ptr<domain::gameplay::Game>>,
                               core::Error>
    list_active() override;

private:
    struct GameHeader {
        std::uint32_t id{};
        std::uint32_t host{};
        std::string   client_tag;
        std::string   name;
        std::string   map;
        std::uint8_t  max_players{};
        std::uint8_t  state{};
    };

    [[nodiscard]] core::Result<std::shared_ptr<domain::gameplay::Game>,
                               core::Error>
    load_game(std::string_view where_sql, DbParamValue key) const;

    [[nodiscard]] core::Result<std::shared_ptr<domain::gameplay::Game>,
                               core::Error>
    build_game(const GameHeader& h) const;

    [[nodiscard]] core::Result<std::vector<domain::AccountId>, core::Error>
    load_players(std::uint32_t game_id) const;

    std::shared_ptr<IDbDriver> driver_;
};

}  // namespace pvpgn::infra::persistence
