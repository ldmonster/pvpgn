// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/persistence/game_repository.hpp"

#include <optional>
#include <string>

#include "domain/shared/client_tag.hpp"

namespace pvpgn::infra::persistence {

namespace {
bool no_rows(const DbRow&) { return false; }
constexpr std::string_view kCols =
    "id, host, client_tag, name, map, max_players, state";
}  // namespace

core::Result<std::vector<domain::AccountId>, core::Error>
SqlGameRepository::load_players(std::uint32_t game_id) const {
    std::vector<domain::AccountId> players;
    auto q = driver_->query_bind(
        "SELECT account_id FROM game_players WHERE game_id = ? ORDER BY position",
        {static_cast<std::int64_t>(game_id)},
        [&players](const DbRow& row) {
            players.push_back(domain::AccountId{
                static_cast<std::uint32_t>(row.get_int(0))});
            return true;
        });
    if (!q.has_value()) return core::fail(q.error());
    return players;
}

core::Result<std::shared_ptr<domain::gameplay::Game>, core::Error>
SqlGameRepository::build_game(const GameHeader& h) const {
    auto client = domain::ClientTag::parse(h.client_tag);
    if (!client.has_value()) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "game: invalid client_tag in storage"});
    }
    auto players = load_players(h.id);
    if (!players.has_value()) return core::fail(players.error());

    domain::gameplay::GameDescriptor desc{h.name, h.map, h.max_players};
    auto game = domain::gameplay::Game::rehydrate(
        domain::GameId{h.id}, domain::AccountId{h.host}, client.value(),
        std::move(desc), static_cast<domain::gameplay::GameState>(h.state),
        std::move(players.value()));
    return std::make_shared<domain::gameplay::Game>(std::move(game));
}

core::Result<std::shared_ptr<domain::gameplay::Game>, core::Error>
SqlGameRepository::load_game(std::string_view where_sql,
                             DbParamValue key) const {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }
    std::optional<GameHeader> header;
    std::string sql = "SELECT ";
    sql.append(kCols);
    sql += " FROM games WHERE ";
    sql.append(where_sql);

    auto q = driver_->query_bind(sql, {key}, [&header](const DbRow& row) {
        header = GameHeader{static_cast<std::uint32_t>(row.get_int(0)),
                            static_cast<std::uint32_t>(row.get_int(1)),
                            row.get_text(2),
                            row.get_text(3),
                            row.get_text(4),
                            static_cast<std::uint8_t>(row.get_int(5)),
                            static_cast<std::uint8_t>(row.get_int(6))};
        return false;
    });
    if (!q.has_value()) return core::fail(q.error());
    if (!header) {
        return core::fail(
            core::Error{core::StatusCode::NotFound, "game: not found"});
    }
    return build_game(*header);
}

core::Result<std::shared_ptr<domain::gameplay::Game>, core::Error>
SqlGameRepository::find_by_id(std::uint32_t id) {
    return load_game("id = ?", static_cast<std::int64_t>(id));
}

core::Result<std::shared_ptr<domain::gameplay::Game>, core::Error>
SqlGameRepository::find_by_name(std::string_view name) {
    return load_game("name = ?", std::string{name});
}

core::Result<void, core::Error> SqlGameRepository::save(
    const domain::gameplay::Game& game) {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }
    const std::int64_t id = static_cast<std::int64_t>(game.id().value());

    auto begun = driver_->begin_transaction();
    if (!begun.has_value()) return core::fail(begun.error());

    auto upsert = driver_->query_bind(
        "INSERT OR REPLACE INTO games "
        "(id, host, client_tag, name, map, max_players, state) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)",
        {id, static_cast<std::int64_t>(game.host().value()),
         std::string{game.client().text()}, game.descriptor().name,
         game.descriptor().map,
         static_cast<std::int64_t>(game.descriptor().max_players),
         static_cast<std::int64_t>(static_cast<std::uint8_t>(game.state()))},
        no_rows);
    if (!upsert.has_value()) {
        (void)driver_->rollback();
        return core::fail(upsert.error());
    }

    auto del = driver_->query_bind(
        "DELETE FROM game_players WHERE game_id = ?", {id}, no_rows);
    if (!del.has_value()) {
        (void)driver_->rollback();
        return core::fail(del.error());
    }

    std::int64_t position = 0;
    for (const auto player : game.players()) {
        auto ins = driver_->query_bind(
            "INSERT INTO game_players (game_id, account_id, position) "
            "VALUES (?, ?, ?)",
            {id, static_cast<std::int64_t>(player.value()), position}, no_rows);
        if (!ins.has_value()) {
            (void)driver_->rollback();
            return core::fail(ins.error());
        }
        ++position;
    }

    return driver_->commit();
}

core::Result<void, core::Error> SqlGameRepository::remove(
    std::string_view name) {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }
    const std::string name_str{name};

    auto begun = driver_->begin_transaction();
    if (!begun.has_value()) return core::fail(begun.error());

    auto del_players = driver_->query_bind(
        "DELETE FROM game_players WHERE game_id IN "
        "(SELECT id FROM games WHERE name = ?)",
        {name_str}, no_rows);
    if (!del_players.has_value()) {
        (void)driver_->rollback();
        return core::fail(del_players.error());
    }

    auto del_game = driver_->query_bind("DELETE FROM games WHERE name = ?",
                                        {name_str}, no_rows);
    if (!del_game.has_value()) {
        (void)driver_->rollback();
        return core::fail(del_game.error());
    }

    return driver_->commit();
}

core::Result<std::vector<std::shared_ptr<domain::gameplay::Game>>, core::Error>
SqlGameRepository::list_active() {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }
    // "Active" = not yet finalized.
    std::vector<GameHeader> headers;
    std::string sql = "SELECT ";
    sql.append(kCols);
    sql += " FROM games WHERE state <> ?";

    auto q = driver_->query_bind(
        sql,
        {static_cast<std::int64_t>(
            static_cast<std::uint8_t>(domain::gameplay::GameState::Finalized))},
        [&headers](const DbRow& row) {
            headers.push_back(
                GameHeader{static_cast<std::uint32_t>(row.get_int(0)),
                           static_cast<std::uint32_t>(row.get_int(1)),
                           row.get_text(2), row.get_text(3), row.get_text(4),
                           static_cast<std::uint8_t>(row.get_int(5)),
                           static_cast<std::uint8_t>(row.get_int(6))});
            return true;
        });
    if (!q.has_value()) return core::fail(q.error());

    std::vector<std::shared_ptr<domain::gameplay::Game>> games;
    games.reserve(headers.size());
    for (const auto& h : headers) {
        auto game = build_game(h);
        if (!game.has_value()) return core::fail(game.error());
        games.push_back(std::move(game.value()));
    }
    return games;
}

}  // namespace pvpgn::infra::persistence
