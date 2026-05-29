// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2dbs_legacy_prefs.hpp
/// Read-only adapter that mimics the legacy `d2dbs prefs_get_*()` /
/// `d2dbs_prefs_get_*()` accessor surface on top of the typed
/// `D2dbsServerConfig`.
///
/// Mirrors `infra/config/legacy_prefs.hpp` (bnetd) and
/// `infra/config/d2cs_legacy_prefs.hpp` (d2cs). The snapshot is
/// **immutable**; hot-reload constructs a fresh instance.

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "infra/config/d2dbs_server_config.hpp"

namespace pvpgn::infra::config {

class D2dbsLegacyPrefs {
public:
    explicit D2dbsLegacyPrefs(D2dbsServerConfig cfg)
        : cfg_(std::move(cfg))
        , servaddrs_str_(cfg_.network.servaddrs)
        , gameservlist_str_(cfg_.network.gameservlist)
        , loglevels_str_(cfg_.log.levels)
        , logfile_str_(cfg_.files.logfile.string())
        , logfile_gs_str_(cfg_.files.logfile_gs.string())
        , charsave_dir_str_(cfg_.files.charsave_dir.string())
        , charinfo_dir_str_(cfg_.files.charinfo_dir.string())
        , ladder_dir_str_(cfg_.files.ladder_dir.string())
        , bak_charsave_dir_str_(cfg_.files.bak_charsave_dir.string())
        , bak_charinfo_dir_str_(cfg_.files.bak_charinfo_dir.string())
        , pidfile_str_(cfg_.files.pidfile.string())
    {}

    // в”Ђв”Ђ [network] в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    const std::string& servaddrs()             const noexcept { return servaddrs_str_; }
    const std::string& gameservlist()          const noexcept { return gameservlist_str_; }

    // в”Ђв”Ђ [log] в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    const std::string& loglevels()             const noexcept { return loglevels_str_; }

    // в”Ђв”Ђ [files] в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    const std::string& logfile()               const noexcept { return logfile_str_; }
    const std::string& logfile_gs()            const noexcept { return logfile_gs_str_; }
    const std::string& charsave_dir()          const noexcept { return charsave_dir_str_; }
    const std::string& charinfo_dir()          const noexcept { return charinfo_dir_str_; }
    const std::string& ladder_dir()            const noexcept { return ladder_dir_str_; }
    const std::string& bak_charsave_dir()      const noexcept { return bak_charsave_dir_str_; }
    const std::string& bak_charinfo_dir()      const noexcept { return bak_charinfo_dir_str_; }
    const std::string& pidfile()               const noexcept { return pidfile_str_; }

    // в”Ђв”Ђ [ladder] в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    std::uint32_t    laddersave_interval()    const noexcept { return cfg_.ladder.laddersave_interval; }
    std::uint32_t    ladderinit_time()        const noexcept { return cfg_.ladder.ladderinit_time; }
    bool             XML_output_ladder()      const noexcept { return cfg_.ladder.XML_ladder_output; }
    bool             ladder_chars_only()      const noexcept { return cfg_.ladder.ladder_chars_only; }
    std::uint32_t    ladderupdate_threshold() const noexcept { return cfg_.ladder.ladderupdate_threshold; }

    // в”Ђв”Ђ [misc] в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    std::uint32_t    shutdown_delay()         const noexcept { return cfg_.misc.shutdown_delay; }
    std::uint32_t    shutdown_decr()          const noexcept { return cfg_.misc.shutdown_decr; }
    std::uint32_t    idletime()               const noexcept { return cfg_.misc.idletime; }
    std::uint32_t    keepalive_interval()     const noexcept { return cfg_.misc.keepalive_interval; }
    std::uint32_t    timeout_checkinterval()  const noexcept { return cfg_.misc.timeout_checkinterval; }
    std::uint32_t    difficulty_hack()        const noexcept { return cfg_.misc.difficulty_hack; }

    /// Direct access to the underlying typed config -- preferred for new
    /// code; the named accessors above are an *adapter*.
    const D2dbsServerConfig& config() const noexcept { return cfg_; }

private:
    D2dbsServerConfig cfg_;

    std::string servaddrs_str_;
    std::string gameservlist_str_;
    std::string loglevels_str_;
    std::string logfile_str_;
    std::string logfile_gs_str_;
    std::string charsave_dir_str_;
    std::string charinfo_dir_str_;
    std::string ladder_dir_str_;
    std::string bak_charsave_dir_str_;
    std::string bak_charinfo_dir_str_;
    std::string pidfile_str_;
};

inline std::shared_ptr<D2dbsLegacyPrefs> make_d2dbs_legacy_prefs(D2dbsServerConfig cfg) {
    return std::make_shared<D2dbsLegacyPrefs>(std::move(cfg));
}

}  // namespace pvpgn::infra::config
