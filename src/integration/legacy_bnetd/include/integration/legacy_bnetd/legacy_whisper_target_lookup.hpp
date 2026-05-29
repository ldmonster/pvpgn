// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file legacy_whisper_target_lookup.hpp
/// Adapter that resolves whisper target state against the live
/// bnetd_legacy connection list. Batch 22a.

#include <string_view>

#include "application/chat/whisper_target_lookup.hpp"

namespace pvpgn::integration::legacy_bnetd {

/// Implementation of `pvpgn::application::chat::IWhisperTargetLookup`
/// that consults the legacy `connlist`/`account` globals. Suitable
/// only for builds that link `bnetd_legacy` (guarded by
/// `PVPGN_V3_BNETD_INTEGRATION`).
class LegacyWhisperTargetLookup final
    : public application::chat::IWhisperTargetLookup {
 public:
    LegacyWhisperTargetLookup() noexcept = default;

    /// Resolves `target` against the live connection list.
    /// `sender` is currently unused for ignored-by detection
    /// (legacy bnetd does not expose a stable "is sender ignored
    /// by target" query without scanning the friend list, which
    /// will land in 23a). Today this returns `ignored_by=false`.
    application::chat::WhisperTarget lookup(
        std::string_view sender, std::string_view target) const noexcept override;
};

}  // namespace pvpgn::integration::legacy_bnetd
