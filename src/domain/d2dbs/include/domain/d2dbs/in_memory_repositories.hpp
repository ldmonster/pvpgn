// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file in_memory_repositories.hpp
/// In-memory implementations of D2DBS repository interfaces.
///
/// These implementations are header-only (all inline) and intended for use
/// in unit tests and development scenarios where no persistent storage is
/// needed.
///
/// ## InMemoryCharacterSaveRepository
///
/// Backed by `std::map<std::string, CharacterSaveData>` keyed on
/// `"account\0char_name"` composite key.
/// Lock state is tracked in a separate `std::set<std::string>`.
///
/// ## InMemoryD2DBSLadderRepository
///
/// Backed by `std::map<std::string, LadderUpdateEntry>` keyed on char_name.

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "domain/d2dbs/character_save_repository.hpp"
#include "domain/d2dbs/ladder_repository.hpp"
#include "domain/d2dbs/types.hpp"

namespace pvpgn::domain::d2dbs {

// ---------------------------------------------------------------------------
// InMemoryCharacterSaveRepository
// ---------------------------------------------------------------------------

/// In-memory implementation of `ICharacterSaveRepository`.
///
/// Thread-safety: none — single-threaded use only (unit tests).
class InMemoryCharacterSaveRepository final : public ICharacterSaveRepository {
public:
    InMemoryCharacterSaveRepository() = default;

    // -----------------------------------------------------------------------
    // ICharacterSaveRepository interface
    // -----------------------------------------------------------------------

    [[nodiscard]] std::optional<CharacterSaveData>
    load(std::string_view account, std::string_view char_name) const override {
        auto it = saves_.find(make_key(account, char_name));
        if (it == saves_.end()) return std::nullopt;
        return it->second;
    }

    [[nodiscard]] bool save(const CharacterSaveData& data) override {
        saves_[make_key(data.account_name, data.char_name)] = data;
        return true;
    }

    [[nodiscard]] bool
    lock(std::string_view account, std::string_view char_name) override {
        const auto key = make_key(account, char_name);
        // Character must exist to be locked
        if (saves_.find(key) == saves_.end()) return false;
        // Already locked — cannot acquire again
        if (locks_.count(key) > 0) return false;
        locks_.insert(key);
        return true;
    }

    [[nodiscard]] bool
    unlock(std::string_view account, std::string_view char_name) override {
        const auto key = make_key(account, char_name);
        auto it = locks_.find(key);
        if (it == locks_.end()) return false;
        locks_.erase(it);
        return true;
    }

    [[nodiscard]] CharacterLockState
    lock_state(std::string_view account, std::string_view char_name) const override {
        const auto key = make_key(account, char_name);
        if (locks_.count(key) > 0) return CharacterLockState::Locked;
        return CharacterLockState::Unlocked;
    }

    // -----------------------------------------------------------------------
    // Test helpers
    // -----------------------------------------------------------------------

    /// Return the total number of saved characters.
    [[nodiscard]] std::size_t save_count() const noexcept {
        return saves_.size();
    }

    /// Return the number of currently locked characters.
    [[nodiscard]] std::size_t lock_count() const noexcept {
        return locks_.size();
    }

    /// Clear all data (useful between test cases).
    void clear() noexcept {
        saves_.clear();
        locks_.clear();
    }

private:
    std::map<std::string, CharacterSaveData> saves_;
    std::set<std::string>                    locks_;

    /// Build a composite key from account and character name.
    ///
    /// Uses a null byte as separator — safe because neither field contains
    /// null bytes in valid D2 data.
    [[nodiscard]] static std::string
    make_key(std::string_view account, std::string_view char_name) {
        std::string key;
        key.reserve(account.size() + 1 + char_name.size());
        key.append(account);
        key.push_back('\0');
        key.append(char_name);
        return key;
    }
};

// ---------------------------------------------------------------------------
// InMemoryD2DBSLadderRepository
// ---------------------------------------------------------------------------

/// In-memory implementation of `ID2DBSLadderRepository`.
///
/// Entries are stored in a map keyed by character name.
///
/// Thread-safety: none — single-threaded use only (unit tests).
class InMemoryD2DBSLadderRepository final : public ID2DBSLadderRepository {
public:
    InMemoryD2DBSLadderRepository() = default;

    // -----------------------------------------------------------------------
    // ID2DBSLadderRepository interface
    // -----------------------------------------------------------------------

    [[nodiscard]] bool update_entry(const LadderUpdateEntry& entry) override {
        entries_[entry.char_name] = entry;
        return true;
    }

    [[nodiscard]] std::optional<LadderUpdateEntry>
    find_entry(std::string_view char_name) const override {
        auto it = entries_.find(std::string(char_name));
        if (it == entries_.end()) return std::nullopt;
        return it->second;
    }

    // -----------------------------------------------------------------------
    // Test helpers
    // -----------------------------------------------------------------------

    /// Return the total number of ladder entries.
    [[nodiscard]] std::size_t entry_count() const noexcept {
        return entries_.size();
    }

    /// Clear all data (useful between test cases).
    void clear() noexcept {
        entries_.clear();
    }

private:
    std::map<std::string, LadderUpdateEntry> entries_;
};

} // namespace pvpgn::domain::d2dbs
