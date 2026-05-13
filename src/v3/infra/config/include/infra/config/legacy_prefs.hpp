// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file legacy_prefs.hpp
/// Read-only snapshot that mimics the surface of the legacy
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

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "infra/config/server_config.hpp"

namespace pvpgn::infra::config {

class LegacyPrefs {
public:
    explicit LegacyPrefs(ServerConfig cfg)
        : cfg_(std::move(cfg)),
          script_dir_(cfg_.script_dir.string()),
          log_file_(cfg_.log.file.string()) {}

    // --- "prefs" surface (subset; widen as legacy callers are ported) ----

    std::string_view servername()      const noexcept { return cfg_.servername;          }
    std::string_view bind_addr()       const noexcept { return cfg_.network.bind_addr;   }
    std::uint16_t    port()            const noexcept { return cfg_.network.port;        }
    std::string_view script_dir()      const noexcept { return script_dir_;              }
    std::string_view storage_driver()  const noexcept { return cfg_.storage.driver;      }
    std::string_view storage_dsn()     const noexcept { return cfg_.storage.dsn;         }
    std::uint32_t    storage_pool()    const noexcept { return cfg_.storage.pool;        }
    core::LogLevel   log_level()       const noexcept { return cfg_.log.level;           }
    std::string_view log_file()        const noexcept { return log_file_;                }
    bool             log_stdout()      const noexcept { return cfg_.log.stdout_sink;     }

    /// Direct access to the underlying typed config — preferred for new
    /// code; the named accessors above are an *adapter*, not the API.
    const ServerConfig& config() const noexcept { return cfg_; }

private:
    ServerConfig cfg_;
    std::string  script_dir_;
    std::string  log_file_;
};

inline std::shared_ptr<LegacyPrefs> make_legacy_prefs(ServerConfig cfg) {
    return std::make_shared<LegacyPrefs>(std::move(cfg));
}

}  // namespace pvpgn::infra::config
