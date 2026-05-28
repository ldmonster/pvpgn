// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file legacy_help_corpus_provider.hpp
/// `IHelpCorpusProvider` adapter that lazily loads `.help` files from
/// disk using legacy prefs + `i18n_filename` + the legacy
/// `languages` array, parses them via `parse_help_corpus`, and caches
/// one `HelpCorpus` per locale.
///
/// First-call init is guarded by `std::call_once`. The cache is keyed
/// by `t_gamelang` (a 4-byte tag). Connections whose gamelang has no
/// loaded corpus fall back to the first configured language (the
/// same fallback policy as legacy `get_hfd`).

#include <map>
#include <mutex>

#include "application/admin_commands/help_corpus.hpp"
#include "application/admin_commands/help_corpus_provider.hpp"

namespace pvpgn::integration::legacy_bnetd {

class LegacyHelpCorpusProvider final
    : public application::admin_commands::IHelpCorpusProvider {
public:
    const application::admin_commands::HelpCorpus*
    for_connection(void* connection) const override;

private:
    void ensure_loaded() const;
    void load_all() const;

    mutable std::once_flag init_;
    // Using unsigned (rather than legacy `t_gamelang`) so this
    // header stays legacy-free. The .cpp casts at load time.
    mutable std::map<unsigned, application::admin_commands::HelpCorpus> corpora_;
    mutable unsigned default_lang_ = 0;
    mutable bool     default_set_ = false;
};

}  // namespace pvpgn::integration::legacy_bnetd
