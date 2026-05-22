// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file in_memory_repositories.hpp
/// In-memory implementations of D2CS repository interfaces.
///
/// These implementations are header-only (all inline) and intended for use
/// in unit tests and development scenarios where no persistent storage is
/// needed.
///
/// ## InMemoryCharacterRepository
///
/// Backed by `std::map<std::string, std::vector<CharacterInfo>>`.
/// Key = account name (case-sensitive).
/// Characters within an account are stored in insertion order.
///
/// ## InMemoryLadderRepository
///
/// Backed by `std::map<LadderType, std::vector<LadderEntry>>`.
/// Entries are kept sorted by `experience` descending (rank 1 = highest XP).
/// Ranks are recomputed on every `add_entry()` / `remove_entry()` call.

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "domain/d2cs/character_repository.hpp"
#include "domain/d2cs/ladder_repository.hpp"
#include "domain/d2cs/types.hpp"

namespace pvpgn::domain::d2cs {

// ---------------------------------------------------------------------------
// InMemoryCharacterRepository
// ---------------------------------------------------------------------------

/// In-memory implementation of `ICharacterRepository`.
///
/// Thread-safety: none — single-threaded use only (unit tests).
class InMemoryCharacterRepository final : public ICharacterRepository {
public:
    InMemoryCharacterRepository() = default;

    // -----------------------------------------------------------------------
    // ICharacterRepository interface
    // -----------------------------------------------------------------------

    [[nodiscard]] std::optional<std::vector<CharacterInfo>>
    list_characters(std::string_view account_name) const override {
        auto it = accounts_.find(std::string(account_name));
        if (it == accounts_.end()) {
            // Account not found — return empty list (not nullopt).
            // An account with no characters is different from a missing account
            // only in the context of explicit account creation; for D2CS the
            // distinction is not meaningful at the domain level.
            return std::vector<CharacterInfo>{};
        }
        return it->second;
    }

    [[nodiscard]] std::optional<CharacterInfo>
    find_character(std::string_view account_name,
                   std::string_view char_name) const override {
        auto it = accounts_.find(std::string(account_name));
        if (it == accounts_.end()) return std::nullopt;

        const auto& chars = it->second;
        auto cit = std::find_if(chars.begin(), chars.end(),
            [&](const CharacterInfo& c) { return c.name == char_name; });
        if (cit == chars.end()) return std::nullopt;
        return *cit;
    }

    [[nodiscard]] bool
    save_character(std::string_view account_name,
                   const CharacterInfo& info) override {
        auto& chars = accounts_[std::string(account_name)];
        auto it = std::find_if(chars.begin(), chars.end(),
            [&](const CharacterInfo& c) { return c.name == info.name; });
        if (it != chars.end()) {
            *it = info;  // update existing
        } else {
            chars.push_back(info);  // insert new
        }
        return true;
    }

    [[nodiscard]] bool
    delete_character(std::string_view account_name,
                     std::string_view char_name) override {
        auto it = accounts_.find(std::string(account_name));
        if (it == accounts_.end()) return false;

        auto& chars = it->second;
        auto cit = std::find_if(chars.begin(), chars.end(),
            [&](const CharacterInfo& c) { return c.name == char_name; });
        if (cit == chars.end()) return false;

        chars.erase(cit);
        return true;
    }

    // -----------------------------------------------------------------------
    // Test helpers
    // -----------------------------------------------------------------------

    /// Return the total number of characters across all accounts.
    [[nodiscard]] std::size_t total_character_count() const noexcept {
        std::size_t n = 0;
        for (const auto& [_, chars] : accounts_) n += chars.size();
        return n;
    }

    /// Return the number of accounts stored.
    [[nodiscard]] std::size_t account_count() const noexcept {
        return accounts_.size();
    }

    /// Clear all data (useful between test cases).
    void clear() noexcept { accounts_.clear(); }

private:
    std::map<std::string, std::vector<CharacterInfo>> accounts_;
};

// ---------------------------------------------------------------------------
// InMemoryLadderRepository
// ---------------------------------------------------------------------------

/// In-memory implementation of `ILadderRepository`.
///
/// Entries are stored sorted by `experience` descending.
/// Ranks are 1-based and recomputed whenever the ladder is modified.
///
/// Thread-safety: none — single-threaded use only (unit tests).
class InMemoryLadderRepository final : public ILadderRepository {
public:
    InMemoryLadderRepository() = default;

    // -----------------------------------------------------------------------
    // ILadderRepository interface
    // -----------------------------------------------------------------------

    [[nodiscard]] std::optional<std::vector<LadderEntry>>
    get_ladder(LadderType type,
               uint32_t   start_pos,
               uint32_t   count) const override {
        auto it = ladders_.find(type);
        if (it == ladders_.end()) {
            return std::vector<LadderEntry>{};
        }
        const auto& entries = it->second;
        if (start_pos >= static_cast<uint32_t>(entries.size())) {
            return std::vector<LadderEntry>{};
        }
        auto begin = entries.begin() + static_cast<std::ptrdiff_t>(start_pos);
        auto end   = entries.end();
        if (count < static_cast<uint32_t>(std::distance(begin, end))) {
            end = begin + static_cast<std::ptrdiff_t>(count);
        }
        return std::vector<LadderEntry>(begin, end);
    }

    [[nodiscard]] std::optional<LadderEntry>
    get_character_ladder_entry(std::string_view char_name,
                               LadderType       type) const override {
        auto it = ladders_.find(type);
        if (it == ladders_.end()) return std::nullopt;

        const auto& entries = it->second;
        auto eit = std::find_if(entries.begin(), entries.end(),
            [&](const LadderEntry& e) { return e.character_name == char_name; });
        if (eit == entries.end()) return std::nullopt;
        return *eit;
    }

    // -----------------------------------------------------------------------
    // Mutation helpers (for test setup)
    // -----------------------------------------------------------------------

    /// Add or update a ladder entry.
    ///
    /// If an entry with the same `character_name` already exists on the given
    /// ladder it is replaced; otherwise a new entry is appended.
    /// Ranks are recomputed after the operation.
    void add_entry(LadderType type, LadderEntry entry) {
        auto& entries = ladders_[type];
        auto it = std::find_if(entries.begin(), entries.end(),
            [&](const LadderEntry& e) {
                return e.character_name == entry.character_name;
            });
        if (it != entries.end()) {
            *it = std::move(entry);
        } else {
            entries.push_back(std::move(entry));
        }
        rerank(entries);
    }

    /// Remove a ladder entry by character name.
    ///
    /// @return `true` if the entry was found and removed; `false` otherwise.
    bool remove_entry(LadderType type, std::string_view char_name) {
        auto it = ladders_.find(type);
        if (it == ladders_.end()) return false;

        auto& entries = it->second;
        auto eit = std::find_if(entries.begin(), entries.end(),
            [&](const LadderEntry& e) { return e.character_name == char_name; });
        if (eit == entries.end()) return false;

        entries.erase(eit);
        rerank(entries);
        return true;
    }

    /// Return the number of entries on the given ladder.
    [[nodiscard]] std::size_t entry_count(LadderType type) const noexcept {
        auto it = ladders_.find(type);
        if (it == ladders_.end()) return 0;
        return it->second.size();
    }

    /// Clear all ladder data.
    void clear() noexcept { ladders_.clear(); }

private:
    /// Sort entries by experience descending and assign 1-based ranks.
    static void rerank(std::vector<LadderEntry>& entries) {
        std::sort(entries.begin(), entries.end(),
            [](const LadderEntry& a, const LadderEntry& b) {
                return a.experience > b.experience;
            });
        uint32_t rank = 1;
        for (auto& e : entries) {
            e.rank = rank++;
        }
    }

    std::map<LadderType, std::vector<LadderEntry>> ladders_;
};

} // namespace pvpgn::domain::d2cs
