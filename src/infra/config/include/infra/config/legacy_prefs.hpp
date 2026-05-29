// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file legacy_prefs.hpp
/// Read-only snapshot that mimics the full surface of the legacy
/// `prefs_get_*()` accessor family on top of the typed `ServerConfig`.
///
/// Purpose: during the migration window the new tree exposes the same
/// names the legacy callers know (`servername()`, `bind_addr()`,
/// `port()`, ...) so that adapters can be written without each one
/// reaching into `ServerConfig` internals. When a subsystem is fully
/// ported, it depends directly on `ServerConfig` and the adapter call
/// goes away with it.
///
/// The snapshot is **immutable**. Hot-reload returns a fresh
/// `LegacyPrefs` instance; subscribers swap atomically.
///
/// Round 121: expanded to cover all ~100 `prefs_get_*` accessors from
/// `src/bnetd/prefs.h`, matching every field in the expanded
/// `ServerConfig` struct.
///
/// Round 331: Secret fields (`storage_dsn`, `wol_autoupdate_password`)
/// now call `.reveal()` to extract the underlying string.
///
/// TODO(R333): retire this shim once all three consumers are ported to
/// use `ServerConfig` directly:
///   - src/v3/integration/legacy_bnetd/src/prefs_bridge.cpp
///   - src/v3/infra/config/include/infra/config/prefs_dump.hpp
///   - src/v3/integration/legacy_bnetd/src/d2dbs_prefs_bridge.cpp

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "infra/config/server_config.hpp"

namespace pvpgn::infra::config {

class LegacyPrefs {
public:
    explicit LegacyPrefs(ServerConfig cfg)
        : cfg_(std::move(cfg))
        // Pre-compute std::string copies for fields that return const char*
        // in the legacy API (filesystem::path → string).
        , storage_path_str_(cfg_.storage.path)
        , filedir_str_(cfg_.files.filedir.string())
        , i18ndir_str_(cfg_.files.i18ndir.string())
        , logfile_str_(cfg_.files.logfile.string())
        , localizefile_str_(cfg_.localization.localizefile)
        , motdfile_str_(cfg_.localization.motdfile)
        , motdw3file_str_(cfg_.localization.motdw3file)
        , newsfile_str_(cfg_.localization.newsfile)
        , channelfile_str_(cfg_.files.channelfile.string())
        , pidfile_str_(cfg_.files.pidfile.string())
        , adfile_str_(cfg_.files.adfile.string())
        , topicfile_str_(cfg_.files.topicfile.string())
        , DBlayoutfile_str_(cfg_.files.DBlayoutfile.string())
        , supportfile_str_(cfg_.files.supportfile.string())
        , reportdir_str_(cfg_.files.reportdir.string())
        , iconfile_str_(cfg_.downloads.iconfile)
        , war3_iconfile_str_(cfg_.downloads.war3_iconfile)
        , star_iconfile_str_(cfg_.downloads.star_iconfile)
        , tosfile_str_(cfg_.localization.tosfile)
        , mpqauthfile_str_(cfg_.downloads.mpqauthfile)
        , mpqfile_str_(cfg_.files.mpqfile.string())
        , trackaddrs_str_(cfg_.tracking.trackserv_addrs)
        , bnetdserv_addrs_str_(cfg_.network.bnetdserv_addrs)
        , w3route_addr_str_(cfg_.network.w3route_addr)
        , ircaddrs_str_(cfg_.irc.irc_addrs)
        , ipbanfile_str_(cfg_.files.ipbanfile.string())
        , helpfile_str_(cfg_.localization.helpfile)
        , transfile_str_(cfg_.files.transfile.string())
        , chanlogdir_str_(cfg_.files.chanlogdir.string())
        , userlogdir_str_(cfg_.files.userlogdir.string())
        , realmfile_str_(cfg_.files.realmfile.string())
        , issuefile_str_(cfg_.files.issuefile.string())
        , effective_user_str_(cfg_.privileges.effective_user)
        , effective_group_str_(cfg_.privileges.effective_group)
        , maildir_str_(cfg_.files.maildir.string())
        , versioncheck_file_str_(cfg_.files.versioncheck_file.string())
        , telnetaddrs_str_(cfg_.telnet.telnet_addrs)
        , mapsfile_str_(cfg_.files.mapsfile.string())
        , xplevelfile_str_(cfg_.files.xplevelfile.string())
        , xpcalcfile_str_(cfg_.files.xpcalcfile.string())
        , ladderdir_str_(cfg_.files.ladderdir.string())
        , statusdir_str_(cfg_.files.statusdir.string())
        , account_allowed_symbols_str_(cfg_.account.account_allowed_symbols)
        , command_groups_file_str_(cfg_.files.command_groups_file.string())
        , tournament_file_str_(cfg_.files.tournament_file.string())
        , customicons_file_str_(cfg_.files.customicons_file.string())
        , script_dir_str_(cfg_.files.scriptdir.string())
        , aliasfile_str_(cfg_.files.aliasfile.string())
        , anongame_infos_file_str_(cfg_.files.anongame_infos_file.string())
        , servername_str_(cfg_.network.servername)
        , hostname_str_(cfg_.network.hostname)
        , location_str_(cfg_.tracking.location)
        , description_str_(cfg_.tracking.description)
        , url_str_(cfg_.tracking.url)
        , contact_name_str_(cfg_.tracking.contact_name)
        , contact_email_str_(cfg_.tracking.contact_email)
        , irc_network_name_str_(cfg_.irc.irc_network_name)
        , log_command_groups_str_(cfg_.command_log.log_command_groups)
        , log_command_list_str_(cfg_.command_log.log_command_list)
        , allowed_clients_str_(cfg_.client_verification.allowed_clients)
        , ladder_games_str_(cfg_.policy.ladder_games)
        , ladder_prefix_str_(cfg_.policy.ladder_prefix)
        , apireg_addrs_str_(cfg_.wol.apireg_addrs)
        , wgameres_addrs_str_(cfg_.wol.wgameres_addrs)
        , wolv1_addrs_str_(cfg_.wol.wolv1_addrs)
        , wolv2_addrs_str_(cfg_.wol.wolv2_addrs)
        , wol_timezone_str_(cfg_.wol.wol_timezone)
        , wol_longitude_str_(cfg_.wol.wol_longitude)
        , wol_latitude_str_(cfg_.wol.wol_latitude)
        , wol_autoupdate_serverhost_str_(cfg_.wol.wol_autoupdate_serverhost)
        , wol_autoupdate_username_str_(cfg_.wol.wol_autoupdate_username)
        , wol_autoupdate_password_str_(cfg_.wol.wol_autoupdate_password.reveal())
        , loglevels_str_(cfg_.log.levels_str)
    {}

    // ── [storage] ─────────────────────────────────────────────────────────────
    std::string_view storage_path()       const noexcept { return storage_path_str_;    }

    // ── [files] ───────────────────────────────────────────────────────────────
    std::string_view filedir()            const noexcept { return filedir_str_;         }
    std::string_view i18ndir()            const noexcept { return i18ndir_str_;         }
    std::string_view logfile()            const noexcept { return logfile_str_;         }
    std::string_view channelfile()        const noexcept { return channelfile_str_;     }
    std::string_view pidfile()            const noexcept { return pidfile_str_;         }
    std::string_view adfile()             const noexcept { return adfile_str_;          }
    std::string_view topicfile()          const noexcept { return topicfile_str_;       }
    std::string_view DBlayoutfile()       const noexcept { return DBlayoutfile_str_;    }
    std::string_view supportfile()        const noexcept { return supportfile_str_;     }
    std::string_view reportdir()          const noexcept { return reportdir_str_;       }
    std::string_view mpqfile()            const noexcept { return mpqfile_str_;         }
    std::string_view ipbanfile()          const noexcept { return ipbanfile_str_;       }
    std::string_view transfile()          const noexcept { return transfile_str_;       }
    std::string_view chanlogdir()         const noexcept { return chanlogdir_str_;      }
    std::string_view userlogdir()         const noexcept { return userlogdir_str_;      }
    std::string_view realmfile()          const noexcept { return realmfile_str_;       }
    std::string_view issuefile()          const noexcept { return issuefile_str_;       }
    std::string_view maildir()            const noexcept { return maildir_str_;         }
    std::string_view versioncheck_file()  const noexcept { return versioncheck_file_str_; }
    std::string_view mapsfile()           const noexcept { return mapsfile_str_;        }
    std::string_view xplevelfile()        const noexcept { return xplevelfile_str_;     }
    std::string_view xpcalcfile()         const noexcept { return xpcalcfile_str_;      }
    std::string_view ladderdir()          const noexcept { return ladderdir_str_;       }
    std::string_view outputdir()          const noexcept { return statusdir_str_;       }
    std::string_view command_groups_file() const noexcept { return command_groups_file_str_; }
    std::string_view tournament_file()    const noexcept { return tournament_file_str_; }
    std::string_view customicons_file()   const noexcept { return customicons_file_str_; }
    std::string_view scriptdir()          const noexcept { return script_dir_str_;      }
    std::string_view aliasfile()          const noexcept { return aliasfile_str_;       }
    std::string_view anongame_infos_file() const noexcept { return anongame_infos_file_str_; }

    // ── [localization] ────────────────────────────────────────────────────────
    std::string_view localizefile()       const noexcept { return localizefile_str_;    }
    std::string_view motdfile()           const noexcept { return motdfile_str_;        }
    std::string_view motdw3file()         const noexcept { return motdw3file_str_;      }
    std::string_view newsfile()           const noexcept { return newsfile_str_;        }
    std::string_view helpfile()           const noexcept { return helpfile_str_;        }
    std::string_view tosfile()            const noexcept { return tosfile_str_;         }
    bool             localize_by_country() const noexcept { return cfg_.localization.localize_by_country; }

    // ── [log] ─────────────────────────────────────────────────────────────────
    std::string_view loglevels()          const noexcept { return loglevels_str_;       }
    core::LogLevel   log_level()          const noexcept { return cfg_.log.level;       }
    bool             log_stdout()         const noexcept { return cfg_.log.stdout_sink; }

    // ── [d2cs] ────────────────────────────────────────────────────────────────
    std::uint32_t    d2cs_version()       const noexcept { return cfg_.d2cs.version;       }
    bool             allow_d2cs_setname() const noexcept { return cfg_.d2cs.allow_setname; }

    // ── [downloads] ───────────────────────────────────────────────────────────
    std::string_view iconfile()           const noexcept { return iconfile_str_;        }
    std::string_view war3_iconfile()      const noexcept { return war3_iconfile_str_;   }
    std::string_view star_iconfile()      const noexcept { return star_iconfile_str_;   }
    std::string_view mpqauthfile()        const noexcept { return mpqauthfile_str_;     }

    // ── [client_verification] ─────────────────────────────────────────────────
    std::string_view allowed_clients()        const noexcept { return allowed_clients_str_;        }
    bool             allow_bad_version()      const noexcept { return cfg_.client_verification.allow_bad_version;     }
    bool             allow_unknown_version()  const noexcept { return cfg_.client_verification.allow_unknown_version; }

    // ── [timing] ──────────────────────────────────────────────────────────────
    std::uint32_t    user_sync_timer()        const noexcept { return cfg_.timing.usersync;            }
    std::uint32_t    user_flush_timer()       const noexcept { return cfg_.timing.userflush;           }
    std::uint32_t    user_flush_connected()   const noexcept { return cfg_.timing.userflush_connected ? 1u : 0u; }
    std::uint32_t    user_step()              const noexcept { return cfg_.timing.userstep;            }
    std::uint32_t    latency()                const noexcept { return cfg_.timing.latency;             }
    std::uint32_t    irc_latency()            const noexcept { return cfg_.timing.irc_latency;         }
    std::uint32_t    nullmsg()                const noexcept { return cfg_.timing.nullmsg;             }
    std::uint32_t    shutdown_delay()         const noexcept { return cfg_.timing.shutdown_delay;      }
    std::uint32_t    shutdown_decr()          const noexcept { return cfg_.timing.shutdown_decr;       }
    std::uint32_t    ipban_check_int()        const noexcept { return cfg_.timing.ipban_check_int;     }
    int              initkill_timer()         const noexcept { return static_cast<int>(cfg_.timing.initkill_timer); }

    // ── [policy] ──────────────────────────────────────────────────────────────
    std::uint32_t    allow_new_accounts()     const noexcept { return cfg_.policy.new_accounts ? 1u : 0u;       }
    std::uint32_t    max_accounts()           const noexcept { return cfg_.policy.max_accounts;                  }
    std::uint32_t    kick_old_login()         const noexcept { return cfg_.policy.kick_old_login ? 1u : 0u;     }
    std::uint32_t    ask_new_channel()        const noexcept { return cfg_.policy.ask_new_channel ? 1u : 0u;    }
    std::uint32_t    report_all_games()       const noexcept { return cfg_.policy.report_all_games ? 1u : 0u;   }
    std::uint32_t    report_diablo_games()    const noexcept { return cfg_.policy.report_diablo_games ? 1u : 0u;}
    std::uint32_t    hide_pass_games()        const noexcept { return cfg_.policy.hide_pass_games ? 1u : 0u;    }
    std::uint32_t    hide_started_games()     const noexcept { return cfg_.policy.hide_started_games ? 1u : 0u; }
    std::uint32_t    hide_temp_channels()     const noexcept { return cfg_.policy.hide_temp_channels ? 1u : 0u; }
    std::uint32_t    discisloss()             const noexcept { return cfg_.policy.disc_is_loss ? 1u : 0u;       }
    std::string_view ladder_games()           const noexcept { return ladder_games_str_;                         }
    std::string_view ladder_prefix()          const noexcept { return ladder_prefix_str_;                        }
    std::uint32_t    enable_conn_all()        const noexcept { return cfg_.policy.enable_conn_all ? 1u : 0u;    }
    std::uint32_t    hide_addr()              const noexcept { return cfg_.policy.hide_addr ? 1u : 0u;          }
    std::uint32_t    udptest_port()           const noexcept { return cfg_.policy.udptest_port;                  }
    std::uint32_t    max_conns_per_IP()       const noexcept { return cfg_.policy.max_conns_per_IP;              }
    std::uint32_t    max_connections()        const noexcept { return cfg_.policy.max_connections;               }
    std::uint32_t    packet_limit()           const noexcept { return cfg_.policy.packet_limit;                  }
    std::uint32_t    passfail_count()         const noexcept { return cfg_.policy.passfail_count;                }
    std::uint32_t    passfail_bantime()       const noexcept { return cfg_.policy.passfail_bantime;              }
    std::uint32_t    maxusers_per_channel()   const noexcept { return cfg_.policy.maxusers_per_channel;          }
    int              max_friends()            const noexcept { return static_cast<int>(cfg_.policy.max_friends); }
    std::uint32_t    hashtable_size()         const noexcept { return cfg_.policy.hashtable_size;                }
    std::uint32_t    max_concurrent_logins()  const noexcept { return cfg_.policy.max_concurrent_logins;         }
    std::uint32_t    v3_tcp_session_mode()    const noexcept { return cfg_.policy.v3_tcp_session_mode;           }
    std::uint32_t    ladder_init_rating()     const noexcept { return cfg_.policy.ladder_init_rating;            }

    // ── [account] ─────────────────────────────────────────────────────────────
    std::uint32_t    savebyname()             const noexcept { return cfg_.account.savebyname ? 1u : 0u;             }
    std::uint32_t    sync_on_logoff()         const noexcept { return cfg_.account.sync_on_logoff ? 1u : 0u;         }
    std::string_view account_allowed_symbols() const noexcept { return account_allowed_symbols_str_;                  }
    std::uint32_t    account_force_username() const noexcept { return cfg_.account.account_force_username ? 1u : 0u; }
    std::uint32_t    mail_support()           const noexcept { return cfg_.account.mail_support ? 1u : 0u;           }
    std::uint32_t    mail_quota()             const noexcept { return cfg_.account.mail_quota;                        }

    // ── [tracking] ────────────────────────────────────────────────────────────
    std::uint32_t    track()                  const noexcept { return cfg_.tracking.track;          }
    std::string_view trackserv_addrs()        const noexcept { return trackaddrs_str_;              }
    std::string_view location()               const noexcept { return location_str_;                }
    std::string_view description()            const noexcept { return description_str_;             }
    std::string_view url()                    const noexcept { return url_str_;                     }
    std::string_view contact_name()           const noexcept { return contact_name_str_;            }
    std::string_view contact_email()          const noexcept { return contact_email_str_;           }

    // ── [network] ─────────────────────────────────────────────────────────────
    std::string_view servername()             const noexcept { return servername_str_;              }
    std::string_view hostname()               const noexcept { return hostname_str_;                }
    std::string_view bind_addr()              const noexcept { return cfg_.network.bind_addr;       }
    std::uint16_t    port()                   const noexcept { return cfg_.network.port;            }
    std::string_view bnetdserv_addrs()        const noexcept { return bnetdserv_addrs_str_;         }
    std::string_view w3route_addr()           const noexcept { return w3route_addr_str_;            }
    std::uint32_t    use_keepalive()          const noexcept { return cfg_.network.use_keepalive ? 1u : 0u; }
    std::uint32_t    chanlog()                const noexcept { return cfg_.network.chanlog ? 1u : 0u; }

    // ── [wol] ─────────────────────────────────────────────────────────────────
    std::string_view apireg_addrs()                const noexcept { return apireg_addrs_str_;               }
    std::string_view wgameres_addrs()              const noexcept { return wgameres_addrs_str_;             }
    std::string_view wolv1_addrs()                 const noexcept { return wolv1_addrs_str_;                }
    std::string_view wolv2_addrs()                 const noexcept { return wolv2_addrs_str_;                }
    std::string_view wol_timezone()                const noexcept { return wol_timezone_str_;               }
    std::string_view wol_longitude()               const noexcept { return wol_longitude_str_;              }
    std::string_view wol_latitude()                const noexcept { return wol_latitude_str_;               }
    std::string_view wol_autoupdate_serverhost()   const noexcept { return wol_autoupdate_serverhost_str_;  }
    std::string_view wol_autoupdate_username()     const noexcept { return wol_autoupdate_username_str_;    }
    std::string_view wol_autoupdate_password()     const noexcept { return wol_autoupdate_password_str_;    }

    // ── [irc] ─────────────────────────────────────────────────────────────────
    std::string_view irc_addrs()              const noexcept { return ircaddrs_str_;                }
    std::string_view irc_network_name()       const noexcept { return irc_network_name_str_;        }

    // ── [telnet] ──────────────────────────────────────────────────────────────
    std::string_view telnet_addrs()           const noexcept { return telnetaddrs_str_;             }

    // ── [ladder] ──────────────────────────────────────────────────────────────
    int              war3_ladder_update_secs() const noexcept { return static_cast<int>(cfg_.ladder.war3_ladder_update_secs); }
    int              XML_output_ladder()       const noexcept { return cfg_.ladder.XML_output_ladder ? 1 : 0; }

    // ── [status] ──────────────────────────────────────────────────────────────
    int              output_update_secs()     const noexcept { return static_cast<int>(cfg_.status.output_update_secs); }
    int              XML_status_output()      const noexcept { return cfg_.status.XML_status_output ? 1 : 0; }

    // ── [clan] ────────────────────────────────────────────────────────────────
    std::uint32_t    clan_newer_time()              const noexcept { return cfg_.clan.clan_newer_time;              }
    std::uint32_t    clan_max_members()             const noexcept { return cfg_.clan.clan_max_members;             }
    std::uint32_t    clan_channel_default_private() const noexcept { return cfg_.clan.clan_channel_default_private ? 1u : 0u; }
    std::uint32_t    clan_min_invites()             const noexcept { return cfg_.clan.clan_min_invites;             }

    // ── [command_log] ─────────────────────────────────────────────────────────
    std::uint32_t    log_commands()           const noexcept { return cfg_.command_log.log_commands ? 1u : 0u; }
    std::string_view log_command_groups()     const noexcept { return log_command_groups_str_;                  }
    std::string_view log_command_list()       const noexcept { return log_command_list_str_;                    }
    std::string_view log_notice()             const noexcept { return cfg_.command_log.log_notice;              }

    // ── [messages] (chat quota / flood control) ───────────────────────────────
    std::uint32_t    quota()                  const noexcept { return cfg_.messages.quota ? 1u : 0u; }
    std::uint32_t    quota_lines()            const noexcept { return cfg_.messages.quota_lines;    }
    std::uint32_t    quota_time()             const noexcept { return cfg_.messages.quota_time;     }
    std::uint32_t    quota_wrapline()         const noexcept { return cfg_.messages.quota_wrapline; }
    std::uint32_t    quota_maxline()          const noexcept { return cfg_.messages.quota_maxline;  }
    std::uint32_t    quota_dobae()            const noexcept { return cfg_.messages.quota_dobae;    }

    // ── [privileges] ──────────────────────────────────────────────────────────
    std::string_view effective_user()         const noexcept { return effective_user_str_;          }
    std::string_view effective_group()        const noexcept { return effective_group_str_;         }

    // ── storage driver (old compat) ───────────────────────────────────────────
    std::string_view storage_driver()         const noexcept { return cfg_.storage.driver;          }
    std::string_view storage_dsn()            const noexcept { return cfg_.storage.dsn.reveal();    }
    std::uint32_t    storage_pool()           const noexcept { return cfg_.storage.pool;            }

    /// Direct access to the underlying typed config — preferred for new
    /// code; the named accessors above are an *adapter*, not the API.
    const ServerConfig& config() const noexcept { return cfg_; }

private:
    ServerConfig cfg_;

    // Pre-computed string copies (filesystem::path → std::string, etc.)
    std::string storage_path_str_;
    std::string filedir_str_;
    std::string i18ndir_str_;
    std::string logfile_str_;
    std::string localizefile_str_;
    std::string motdfile_str_;
    std::string motdw3file_str_;
    std::string newsfile_str_;
    std::string channelfile_str_;
    std::string pidfile_str_;
    std::string adfile_str_;
    std::string topicfile_str_;
    std::string DBlayoutfile_str_;
    std::string supportfile_str_;
    std::string reportdir_str_;
    std::string iconfile_str_;
    std::string war3_iconfile_str_;
    std::string star_iconfile_str_;
    std::string tosfile_str_;
    std::string mpqauthfile_str_;
    std::string mpqfile_str_;
    std::string trackaddrs_str_;
    std::string bnetdserv_addrs_str_;
    std::string w3route_addr_str_;
    std::string ircaddrs_str_;
    std::string ipbanfile_str_;
    std::string helpfile_str_;
    std::string transfile_str_;
    std::string chanlogdir_str_;
    std::string userlogdir_str_;
    std::string realmfile_str_;
    std::string issuefile_str_;
    std::string effective_user_str_;
    std::string effective_group_str_;
    std::string maildir_str_;
    std::string versioncheck_file_str_;
    std::string telnetaddrs_str_;
    std::string mapsfile_str_;
    std::string xplevelfile_str_;
    std::string xpcalcfile_str_;
    std::string ladderdir_str_;
    std::string statusdir_str_;
    std::string account_allowed_symbols_str_;
    std::string command_groups_file_str_;
    std::string tournament_file_str_;
    std::string customicons_file_str_;
    std::string script_dir_str_;
    std::string aliasfile_str_;
    std::string anongame_infos_file_str_;
    std::string servername_str_;
    std::string hostname_str_;
    std::string location_str_;
    std::string description_str_;
    std::string url_str_;
    std::string contact_name_str_;
    std::string contact_email_str_;
    std::string irc_network_name_str_;
    std::string log_command_groups_str_;
    std::string log_command_list_str_;
    std::string allowed_clients_str_;
    std::string ladder_games_str_;
    std::string ladder_prefix_str_;
    std::string apireg_addrs_str_;
    std::string wgameres_addrs_str_;
    std::string wolv1_addrs_str_;
    std::string wolv2_addrs_str_;
    std::string wol_timezone_str_;
    std::string wol_longitude_str_;
    std::string wol_latitude_str_;
    std::string wol_autoupdate_serverhost_str_;
    std::string wol_autoupdate_username_str_;
    std::string wol_autoupdate_password_str_;
    std::string loglevels_str_;
};

inline std::shared_ptr<LegacyPrefs> make_legacy_prefs(ServerConfig cfg) {
    return std::make_shared<LegacyPrefs>(std::move(cfg));
}

}  // namespace pvpgn::infra::config
