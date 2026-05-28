// SPDX-License-Identifier: GPL-2.0-or-later

#include "integration/legacy_bnetd/legacy_help_corpus_provider.hpp"

#include <fstream>

#include "application/admin_commands/help_corpus.hpp"
#include "application/admin_commands/help_corpus_parser.hpp"
#include "core/logging.hpp"

#include "common/setup_before.h"
#include "common/eventlog.h"
#include "connection.h"
#include "i18n.h"
#include "prefs_v3_shim.h"
#include "common/setup_after.h"

namespace pvpgn::integration::legacy_bnetd {

namespace ac = pvpgn::application::admin_commands;

void LegacyHelpCorpusProvider::ensure_loaded() const
{
    std::call_once(init_, [this] { load_all(); });
}

void LegacyHelpCorpusProvider::load_all() const
{
    const char* filename = ::pvpgn::bnetd::prefs_v3::helpfile();
    if (filename == nullptr || filename[0] == '\0') {
        ::pvpgn::eventlog(
            ::pvpgn::eventlog_level_error, __FUNCTION__,
            "no helpfile path configured; pure-v3 help responder disabled");
        return;
    }

    const auto& langs = ::pvpgn::bnetd::languages;
    if (langs.empty()) {
        ::pvpgn::eventlog(
            ::pvpgn::eventlog_level_error, __FUNCTION__,
            "no languages configured; pure-v3 help responder disabled");
        return;
    }

    bool first = true;
    for (const auto& lang : langs) {
        const std::string path =
            ::pvpgn::bnetd::i18n_filename(filename, lang.gamelang);
        std::ifstream in(path);
        if (!in.is_open()) {
            ::pvpgn::eventlog(
                ::pvpgn::eventlog_level_error, __FUNCTION__,
                "could not open localized help file \"{}\"", path);
            continue;
        }
        auto r = ac::parse_help_corpus(in);
        if (!r) {
            ::pvpgn::eventlog(
                ::pvpgn::eventlog_level_error, __FUNCTION__,
                "parse error in help file \"{}\": {}", path,
                r.error().message());
            continue;
        }
        const auto key = static_cast<unsigned>(lang.gamelang);
        corpora_.emplace(key, std::move(r).value());
        if (first) {
            default_lang_ = key;
            default_set_ = true;
            first = false;
        }
    }
}

const ac::HelpCorpus*
LegacyHelpCorpusProvider::for_connection(void* connection) const
{
    ensure_loaded();
    if (corpora_.empty()) return nullptr;
    if (connection != nullptr) {
        auto* c = static_cast<::pvpgn::bnetd::t_connection*>(connection);
        const auto key = static_cast<unsigned>(
            ::pvpgn::bnetd::conn_get_gamelang_localized(c));
        if (auto it = corpora_.find(key); it != corpora_.end()) {
            return &it->second;
        }
    }
    if (default_set_) {
        if (auto it = corpora_.find(default_lang_); it != corpora_.end()) {
            return &it->second;
        }
    }
    return &corpora_.begin()->second;
}

}  // namespace pvpgn::integration::legacy_bnetd
