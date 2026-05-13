// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_repository.hpp
/// Thread-unsafe in-memory store. Suitable for tests and for the
/// development/CI composition root. Production composition uses a
/// SQL-backed adapter that satisfies the same port.

#include <memory>
#include <string>
#include <unordered_map>

#include "application/ports/account_repository.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryAccountRepository final
    : public application::ports::IAccountRepository {
public:
    core::Result<domain::identity::Account>
    find_by_id(domain::AccountId id) const override {
        auto it = by_id_.find(id.value());
        if (it == by_id_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "account: id not found"});
        }
        return *it->second;
    }

    core::Result<domain::identity::Account>
    find_by_name(const domain::UserName& name) const override {
        auto it = by_canonical_.find(std::string{name.canonical()});
        if (it == by_canonical_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "account: name not found"});
        }
        auto a = by_id_.find(it->second);
        if (a == by_id_.end()) {
            return core::fail(core::Error{
                core::StatusCode::Internal, "account: index corrupt"});
        }
        return *a->second;
    }

    core::Status<> save(const domain::identity::Account& account) override {
        auto copy = std::make_unique<domain::identity::Account>(account);
        by_canonical_[std::string{copy->name().canonical()}] =
            account.id().value();
        by_id_[account.id().value()] = std::move(copy);
        return core::ok();
    }

    core::Status<> remove(domain::AccountId id) override {
        auto it = by_id_.find(id.value());
        if (it == by_id_.end()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "account: id not found"});
        }
        by_canonical_.erase(std::string{it->second->name().canonical()});
        by_id_.erase(it);
        return core::ok();
    }

    std::size_t size() const noexcept override { return by_id_.size(); }

private:
    std::unordered_map<std::uint32_t,
                       std::unique_ptr<domain::identity::Account>> by_id_;
    std::unordered_map<std::string, std::uint32_t> by_canonical_;
};

}  // namespace pvpgn::infra::inmemory
