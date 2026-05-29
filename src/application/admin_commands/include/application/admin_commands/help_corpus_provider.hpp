// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file help_corpus_provider.hpp
/// Port returning the parsed help corpus appropriate for a given
/// connection (typically dispatching by locale).
///
/// The legacy implementation -- `LegacyHelpCorpusProvider` in
/// `integration_legacy_bnetd_linked` -- lazily opens and parses the
/// localized `.help` files at first use, keyed on
/// `conn_get_gamelang_localized(c)`.

#include "application/admin_commands/help_corpus.hpp"

namespace pvpgn::application::admin_commands {

class IHelpCorpusProvider {
public:
    virtual ~IHelpCorpusProvider() = default;

    /// @return pointer to the corpus to use for replies to this
    ///         connection, or nullptr if no corpus is available
    ///         (e.g. the help files were not loadable). When null,
    ///         the responder reports "no help is available" to the
    ///         client and returns failure so the bridge can fall
    ///         back to legacy dispatch.
    [[nodiscard]] virtual const HelpCorpus* for_connection(void* connection) const = 0;
};

}  // namespace pvpgn::application::admin_commands
