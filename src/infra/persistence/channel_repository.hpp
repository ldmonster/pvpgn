// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file channel_repository.hpp
/// Plan 07: consolidated, driver-parameterized channel repository. Replaces the
/// per-backend channel repositories — the same SQL/logic runs over any
/// `IDbDriver`.

#include <functional>
#include <memory>
#include <string>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/chat/channel.hpp"
#include "domain/chat/ports.hpp"
#include "domain/shared/ids.hpp"
#include "infra/persistence/sql_builder/db_driver.hpp"

namespace pvpgn::infra::persistence {

/// `IChannelRepository` implemented over the backend-agnostic `IDbDriver`.
class SqlChannelRepository final : public domain::chat::IChannelRepository {
public:
    explicit SqlChannelRepository(std::shared_ptr<IDbDriver> driver)
        : driver_(std::move(driver)) {}

    [[nodiscard]] core::Result<domain::chat::Channel>
    find_by_name(const std::string& name) const override;

    [[nodiscard]] core::Result<domain::chat::Channel>
    find_by_id(domain::ChannelId id) const override;

    core::Status<> save(const domain::chat::Channel& channel) override;

    core::Status<> remove(domain::ChannelId id) override;

    void forEach(
        std::function<bool(const domain::chat::Channel&)> predicate)
        const override;

    [[nodiscard]] std::size_t size() const noexcept override;

private:
    static domain::chat::Channel channel_from_row(const DbRow& row);

    std::shared_ptr<IDbDriver> driver_;
};

}  // namespace pvpgn::infra::persistence
