// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2cs_legacy_prefs.hpp
/// Read-only adapter that mimics the legacy `d2cs prefs_get_*()` /
/// `d2cs_prefs_get_*()` accessor surface on top of the typed
/// `D2csServerConfig`.
///
/// Mirrors `infra/config/legacy_prefs.hpp` (the bnetd adapter): the
/// snapshot is **immutable**; hot-reload constructs a fresh
/// `D2csLegacyPrefs` and subscribers swap atomically.
///
/// Field names match the public migration shim `src/d2cs/prefs_v3_shim.h`
/// so callers reading from a shared adapter instance get identical
/// values to callers going through the bridge.

#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <string_view>

#include "infra/config/d2cs_server_config.hpp"

namespace pvpgn::infra::config {

class D2csLegacyPrefs {
public:
    explicit D2csLegacyPrefs(D2csServerConfig cfg)
        : cfg_(std::move(cfg))
        , realmname_str_(cfg_.server.realmname)
        , servaddrs_str_(cfg_.network.servaddrs)
        , gameservlist_str_(cfg_.network.gameservlist)
        , bnetdaddr_str_(cfg_.network.bnetdaddr)
        , account_allowed_symbols_str_(cfg_.realm.account_allowed_symbols)
        , loglevels_str_(cfg_.log.levels)
        , logfile_str_(cfg_.files.logfile.string())
        , charsave_dir_str_(cfg_.files.charsave_dir.string())
        , charinfo_dir_str_(cfg_.files.charinfo_dir.string())
        , bak_charsave_dir_str_(cfg_.files.bak_charsave_dir.string())
        , bak_charinfo_dir_str_(cfg_.files.bak_charinfo_dir.string())
        , ladder_dir_str_(cfg_.files.ladder_dir.string())
        , transfile_str_(cfg_.files.transfile.string())
        , d2gsconffile_str_(cfg_.files.d2gsconffile.string())
        , pidfile_str_(cfg_.files.pidfile.string())
        , newbiefile_amazon_str_(cfg_.files.newbiefile_amazon.string())
        , newbiefile_sorceress_str_(cfg_.files.newbiefile_sorceress.string())
        , newbiefile_necromancer_str_(cfg_.files.newbiefile_necromancer.string())
        , newbiefile_paladin_str_(cfg_.files.newbiefile_paladin.string())
        , newbiefile_barbarian_str_(cfg_.files.newbiefile_barbarian.string())
        , newbiefile_druid_str_(cfg_.files.newbiefile_druid.string())
        , newbiefile_assasin_str_(cfg_.files.newbiefile_assasin.string())
        , motd_str_(cfg_.misc.motd)
        , charlist_sort_str_(cfg_.misc.charlist_sort)
        , charlist_sort_order_str_(cfg_.misc.charlist_sort_order)
        , d2gs_password_str_(cfg_.internal_.d2gs_password)
    {}

    // в”Ђв”Ђ [server] в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    const std::string& realmname()                const noexcept { return realmname_str_; }

    // в”Ђв”Ђ [network] в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    const std::string& servaddrs()                const noexcept { return servaddrs_str_; }
    const std::string& gameservlist()             const noexcept { return gameservlist_str_; }
    const std::string& bnetdaddr()                const noexcept { return bnetdaddr_str_; }
    std::uint32_t    max_connections()          const noexcept { return cfg_.network.max_connections; }

    // в”Ђв”Ђ [realm] в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    std::uint32_t    lod_realm()                const noexcept { return cfg_.realm.lod_realm; }
    bool             allow_convert()            const noexcept { return cfg_.realm.allow_convert; }
    const std::string& account_allowed_symbols()  const noexcept { return account_allowed_symbols_str_; }

    // в”Ђв”Ђ [log] в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    const std::string& loglevels()                const noexcept { return loglevels_str_; }

    // в”Ђв”Ђ [files] в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    const std::string& logfile()                  const noexcept { return logfile_str_; }
    const std::string& charsave_dir()             const noexcept { return charsave_dir_str_; }
    const std::string& charinfo_dir()             const noexcept { return charinfo_dir_str_; }
    const std::string& bak_charsave_dir()         const noexcept { return bak_charsave_dir_str_; }
    const std::string& bak_charinfo_dir()         const noexcept { return bak_charinfo_dir_str_; }
    const std::string& ladder_dir()               const noexcept { return ladder_dir_str_; }
    const std::string& transfile()                const noexcept { return transfile_str_; }
    const std::string& d2gsconffile()             const noexcept { return d2gsconffile_str_; }
    const std::string& pidfile()                  const noexcept { return pidfile_str_; }
    const std::string& newbiefile_amazon()        const noexcept { return newbiefile_amazon_str_; }
    const std::string& newbiefile_sorceress()     const noexcept { return newbiefile_sorceress_str_; }
    const std::string& newbiefile_necromancer()   const noexcept { return newbiefile_necromancer_str_; }
    const std::string& newbiefile_paladin()       const noexcept { return newbiefile_paladin_str_; }
    const std::string& newbiefile_barbarian()     const noexcept { return newbiefile_barbarian_str_; }
    const std::string& newbiefile_druid()         const noexcept { return newbiefile_druid_str_; }
    const std::string& newbiefile_assasin()       const noexcept { return newbiefile_assasin_str_; }

    // в”Ђв”Ђ [misc] в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    const std::string& motd()                     const noexcept { return motd_str_; }
    bool             allow_newchar()            const noexcept { return cfg_.misc.allow_newchar; }
    bool             check_multilogin()         const noexcept { return cfg_.misc.check_multilogin; }
    std::uint32_t    maxchar()                  const noexcept { return cfg_.misc.maxchar; }
    const std::string& charlist_sort()            const noexcept { return charlist_sort_str_; }
    const std::string& charlist_sort_order()      const noexcept { return charlist_sort_order_str_; }
    std::uint32_t    maxgamelist()              const noexcept { return cfg_.misc.maxgamelist; }
    bool             gamelist_showall()         const noexcept { return cfg_.misc.gamelist_showall; }
    bool             hide_pass_games()          const noexcept { return cfg_.misc.hide_pass_games; }
    std::uint32_t    idletime()                 const noexcept { return cfg_.misc.idletime; }
    std::uint32_t    shutdown_delay()           const noexcept { return cfg_.misc.shutdown_delay; }
    std::uint32_t    shutdown_decr()            const noexcept { return cfg_.misc.shutdown_decr; }

    // в”Ђв”Ђ [internal] в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    std::uint32_t    listpurgeinterval()        const noexcept { return cfg_.internal_.listpurgeinterval; }
    std::uint32_t    gqcheckinterval()          const noexcept { return cfg_.internal_.gqcheckinterval; }
    std::uint32_t    s2s_retryinterval()        const noexcept { return cfg_.internal_.s2s_retryinterval; }
    std::uint32_t    s2s_timeout()              const noexcept { return cfg_.internal_.s2s_timeout; }
    std::uint32_t    sq_checkinterval()         const noexcept { return cfg_.internal_.sq_checkinterval; }
    std::uint32_t    sq_timeout()               const noexcept { return cfg_.internal_.sq_timeout; }
    std::uint32_t    d2gs_checksum()            const noexcept { return cfg_.internal_.d2gs_checksum; }
    std::uint32_t    d2gs_version()             const noexcept { return cfg_.internal_.d2gs_version; }
    const std::string& d2gs_password()            const noexcept { return d2gs_password_str_; }
    std::uint32_t    game_maxlifetime()         const noexcept { return cfg_.internal_.game_maxlifetime; }
    std::uint32_t    game_maxlevel()            const noexcept { return cfg_.internal_.game_maxlevel; }
    std::uint32_t    max_game_idletime()        const noexcept { return cfg_.internal_.max_game_idletime; }
    bool             allow_gamelimit()          const noexcept { return cfg_.internal_.allow_gamelimit; }
    std::uint32_t    ladder_refresh_interval()  const noexcept { return cfg_.internal_.ladder_refresh_interval; }
    std::uint32_t    s2s_idletime()             const noexcept { return cfg_.internal_.s2s_idletime; }
    std::uint32_t    s2s_keepalive_interval()   const noexcept { return cfg_.internal_.s2s_keepalive_interval; }
    std::uint32_t    timeout_checkinterval()    const noexcept { return cfg_.internal_.timeout_checkinterval; }
    std::uint32_t    d2gs_restart_delay()       const noexcept { return cfg_.internal_.d2gs_restart_delay; }
    std::time_t      ladder_start_time()        const noexcept { return cfg_.internal_.ladder_start_time; }
    std::uint32_t    char_expire_day()          const noexcept { return cfg_.internal_.char_expire_day; }
    std::uint32_t    ladderlist_count()         const noexcept { return cfg_.internal_.ladderlist_count; }

    /// Direct access to the underlying typed config -- preferred for new
    /// code; the named accessors above are an *adapter*.
    const D2csServerConfig& config() const noexcept { return cfg_; }

private:
    D2csServerConfig cfg_;

    std::string realmname_str_;
    std::string servaddrs_str_;
    std::string gameservlist_str_;
    std::string bnetdaddr_str_;
    std::string account_allowed_symbols_str_;
    std::string loglevels_str_;
    std::string logfile_str_;
    std::string charsave_dir_str_;
    std::string charinfo_dir_str_;
    std::string bak_charsave_dir_str_;
    std::string bak_charinfo_dir_str_;
    std::string ladder_dir_str_;
    std::string transfile_str_;
    std::string d2gsconffile_str_;
    std::string pidfile_str_;
    std::string newbiefile_amazon_str_;
    std::string newbiefile_sorceress_str_;
    std::string newbiefile_necromancer_str_;
    std::string newbiefile_paladin_str_;
    std::string newbiefile_barbarian_str_;
    std::string newbiefile_druid_str_;
    std::string newbiefile_assasin_str_;
    std::string motd_str_;
    std::string charlist_sort_str_;
    std::string charlist_sort_order_str_;
    std::string d2gs_password_str_;
};

inline std::shared_ptr<D2csLegacyPrefs> make_d2cs_legacy_prefs(D2csServerConfig cfg) {
    return std::make_shared<D2csLegacyPrefs>(std::move(cfg));
}

}  // namespace pvpgn::infra::config
