// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/persistence/clan_repository.hpp"

#include <optional>
#include <string>
#include <vector>

#include "domain/shared/client_tag.hpp"
#include "domain/social/clan_rank_wire.hpp"

namespace pvpgn::infra::persistence {

namespace {
bool no_rows(const DbRow&) { return false; }
}  // namespace

core::Result<std::vector<domain::social::ClanMember>, core::Error>
SqlClanRepository::load_members(std::uint32_t clan_id) const {
    std::vector<domain::social::ClanMember> members;
    auto q = driver_->query_bind(
        "SELECT account_id, rank FROM clan_members WHERE clan_id = ? "
        "ORDER BY position",
        {static_cast<std::int64_t>(clan_id)},
        [&members](const DbRow& row) {
            domain::social::ClanMember m{
                domain::AccountId{static_cast<std::uint32_t>(row.get_int(0))},
                domain::social::clan_rank_from_wire(
                    static_cast<std::uint8_t>(row.get_int(1)))};
            members.push_back(m);
            return true;  // collect all members
        });
    if (!q.has_value()) {
        return core::fail(q.error());
    }
    return members;
}

core::Result<std::shared_ptr<domain::social::Clan>, core::Error>
SqlClanRepository::load_clan(std::string_view where_sql,
                             DbParamValue key) const {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }

    std::optional<ClanHeader> header;
    std::string sql = "SELECT id, tag, name, client_tag FROM clans WHERE ";
    sql.append(where_sql);

    auto q = driver_->query_bind(sql, {key}, [&header](const DbRow& row) {
        header = ClanHeader{static_cast<std::uint32_t>(row.get_int(0)),
                            row.get_text(1), row.get_text(2), row.get_text(3)};
        return false;  // unique key — at most one row
    });
    if (!q.has_value()) {
        return core::fail(q.error());
    }
    if (!header) {
        return core::fail(
            core::Error{core::StatusCode::NotFound, "clan: not found"});
    }

    auto client = domain::ClientTag::parse(header->client_tag);
    if (!client.has_value()) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "clan: invalid client_tag in storage"});
    }

    auto members = load_members(header->id);
    if (!members.has_value()) {
        return core::fail(members.error());
    }

    auto clan = domain::social::Clan::rehydrate(
        domain::ClanId{header->id}, header->tag, header->name, client.value(),
        std::move(members.value()));
    return std::make_shared<domain::social::Clan>(std::move(clan));
}

core::Result<std::shared_ptr<domain::social::Clan>, core::Error>
SqlClanRepository::find_by_id(domain::ClanId id) {
    return load_clan("id = ?", static_cast<std::int64_t>(id.value()));
}

core::Result<std::shared_ptr<domain::social::Clan>, core::Error>
SqlClanRepository::find_by_tag(std::string_view tag) {
    return load_clan("tag = ?", std::string{tag});
}

core::Result<std::shared_ptr<domain::social::Clan>, core::Error>
SqlClanRepository::find_by_name(std::string_view name) {
    return load_clan("name = ? COLLATE NOCASE", std::string{name});
}

core::Result<void, core::Error> SqlClanRepository::save(
    const domain::social::Clan& clan) {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }

    const std::int64_t id = static_cast<std::int64_t>(clan.id().value());

    auto begun = driver_->begin_transaction();
    if (!begun.has_value()) {
        return core::fail(begun.error());
    }

    auto upsert = driver_->query_bind(
        "INSERT OR REPLACE INTO clans (id, tag, name, client_tag) "
        "VALUES (?, ?, ?, ?)",
        {id, clan.tag(), clan.name(), std::string{clan.client().text()}},
        no_rows);
    if (!upsert.has_value()) {
        (void)driver_->rollback();
        return core::fail(upsert.error());
    }

    auto del = driver_->query_bind(
        "DELETE FROM clan_members WHERE clan_id = ?", {id}, no_rows);
    if (!del.has_value()) {
        (void)driver_->rollback();
        return core::fail(del.error());
    }

    std::int64_t position = 0;
    for (const auto& member : clan.members()) {
        auto ins = driver_->query_bind(
            "INSERT INTO clan_members (clan_id, account_id, rank, position) "
            "VALUES (?, ?, ?, ?)",
            {id, static_cast<std::int64_t>(member.account.value()),
             static_cast<std::int64_t>(
                 domain::social::clan_rank_to_wire(member.rank)),
             position},
            no_rows);
        if (!ins.has_value()) {
            (void)driver_->rollback();
            return core::fail(ins.error());
        }
        ++position;
    }

    return driver_->commit();
}

core::Result<void, core::Error> SqlClanRepository::remove(
    std::string_view tag) {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }

    const std::string tag_str{tag};

    auto begun = driver_->begin_transaction();
    if (!begun.has_value()) {
        return core::fail(begun.error());
    }

    // Delete the members first (keyed by the clan id resolved from the tag),
    // then the clan row itself.
    auto del_members = driver_->query_bind(
        "DELETE FROM clan_members WHERE clan_id IN "
        "(SELECT id FROM clans WHERE tag = ?)",
        {tag_str}, no_rows);
    if (!del_members.has_value()) {
        (void)driver_->rollback();
        return core::fail(del_members.error());
    }

    auto del_clan = driver_->query_bind("DELETE FROM clans WHERE tag = ?",
                                        {tag_str}, no_rows);
    if (!del_clan.has_value()) {
        (void)driver_->rollback();
        return core::fail(del_clan.error());
    }

    return driver_->commit();
}

}  // namespace pvpgn::infra::persistence
