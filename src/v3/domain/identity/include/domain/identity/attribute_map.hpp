// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file attribute_map.hpp
/// `AttributeMap` — typed wrapper around the legacy stringly-typed
/// account attribute bag (`BNET\\acct\\*`, `Record\\*`, profile keys,
/// etc.). New features prefer first-class aggregate fields, but the
/// long tail of legacy keys is kept here so we can read & migrate
/// snapshots verbatim.
///
/// Pure: emits an `AccountAttributeChanged` event for every mutation.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "domain/shared/events.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain::identity {

class AttributeMap {
public:
    explicit AttributeMap(AccountId owner) : owner_(owner) {}

    static AttributeMap rehydrate(AccountId owner,
                                  std::unordered_map<std::string, std::string> values) {
        AttributeMap a{owner};
        a.values_ = std::move(values);
        return a;
    }

    AccountId owner() const noexcept { return owner_; }
    std::size_t size() const noexcept { return values_.size(); }

    /// Returns the raw value or empty `optional` if unset.
    std::optional<std::string_view> get(std::string_view key) const {
        auto it = values_.find(std::string{key});
        if (it == values_.end()) return std::nullopt;
        return std::string_view{it->second};
    }

    /// Set or overwrite. Returns `true` iff the value actually changed
    /// (idempotent on identical writes — no event emitted).
    bool set(std::string key, std::string value) {
        auto [it, inserted] = values_.try_emplace(key, value);
        if (!inserted) {
            if (it->second == value) return false;
            it->second = value;
        }
        events_.push_back(events::AccountAttributeChanged{owner_, std::move(key), std::move(value)});
        return true;
    }

    bool erase(std::string_view key) {
        return values_.erase(std::string{key}) > 0;
    }

    const std::unordered_map<std::string, std::string>& values() const noexcept {
        return values_;
    }

    std::vector<events::DomainEvent> drain_events() {
        return std::exchange(events_, {});
    }

private:
    AccountId                                       owner_;
    std::unordered_map<std::string, std::string>    values_;
    std::vector<events::DomainEvent>                events_;
};

}  // namespace pvpgn::domain::identity
