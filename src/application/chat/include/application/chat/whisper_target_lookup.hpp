// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file whisper_target_lookup.hpp
/// Port for looking up the live state of a whisper target.
///
/// The pure `decide_whisper` use-case is parameterised on a
/// `WhisperTarget` snapshot of the target's connection. The bridge
/// must produce that snapshot before calling
/// `decide_whisper`. This port is the seam:
///
///   * `LegacyWhisperTargetLookup` queries
///     `connlist_find_connection_by_accountname` + `account_get_dnd`
///     + the sender's ignore list in `bnetd_legacy`.
///   * `MapWhisperTargetLookup` (this header) is a test fake that
///     resolves names against an in-memory `std::unordered_map`.
///   * `NullWhisperTargetLookup` always reports "not online".

#include <cctype>
#include <string>
#include <string_view>
#include <unordered_map>

#include "application/chat/whisper_use_case.hpp"

namespace pvpgn::application::chat {

class IWhisperTargetLookup {
public:
    virtual ~IWhisperTargetLookup() = default;

    /// Resolve `target_name` to a snapshot of its connection state,
    /// from the perspective of `sender_name` (the perspective
    /// matters for the `ignored_by` check, which is per-pair).
    ///
    /// Returning `{online=false}` is the canonical "target not
    /// reachable" answer. Implementations MUST NOT throw.
    virtual WhisperTarget lookup(std::string_view sender_name,
                                 std::string_view target_name) const noexcept = 0;
};

class NullWhisperTargetLookup final : public IWhisperTargetLookup {
public:
    WhisperTarget lookup(std::string_view,
                         std::string_view) const noexcept override {
        return WhisperTarget{};  // online=false, dnd=false, ignored_by=false
    }
};

/// In-memory fake for tests. The map key is the lowercased target
/// name; the optional sender filter on `ignored_by` is intentionally
/// not modelled here (tests that need it should subclass).
class MapWhisperTargetLookup final : public IWhisperTargetLookup {
public:
    void set(std::string target_name, WhisperTarget state) {
        for (auto& c : target_name) {
            c = static_cast<char>(
                std::tolower(static_cast<unsigned char>(c)));
        }
        targets_[std::move(target_name)] = state;
    }

    WhisperTarget lookup(std::string_view /*sender*/,
                         std::string_view target) const noexcept override {
        std::string key(target);
        for (auto& c : key) {
            c = static_cast<char>(
                std::tolower(static_cast<unsigned char>(c)));
        }
        if (auto it = targets_.find(key); it != targets_.end()) {
            return it->second;
        }
        return WhisperTarget{};
    }

private:
    std::unordered_map<std::string, WhisperTarget> targets_;
};

}  // namespace pvpgn::application::chat
