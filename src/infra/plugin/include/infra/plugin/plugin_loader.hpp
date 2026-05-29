// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/**
 * @file plugin_loader.hpp
 * @brief Native plugin loader for PvPGN v3.
 *
 * Loads shared libraries (.so on POSIX, .dll on Windows) that implement the
 * pvpgn plugin C ABI 1.0 defined in api.h.  Each library must export:
 *   - pvpgn_plugin_get_info()
 *   - pvpgn_plugin_init()
 *   - pvpgn_plugin_shutdown()
 */

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "infra/plugin/api.h"

namespace pvpgn::infra::plugin {

/**
 * @brief Represents a successfully loaded native plugin.
 */
struct LoadedPlugin {
    /// Plugin name from pvpgn_plugin_info_t::name.
    std::string name;
    /// Plugin version from pvpgn_plugin_info_t::version.
    std::string version;
    /// OS-level handle returned by dlopen / LoadLibrary.
    void* handle{nullptr};
    /// Pointer to the plugin's shutdown function; called before dlclose.
    pvpgn_plugin_shutdown_fn shutdown{nullptr};
};

/**
 * @brief Loads, initialises, and manages the lifecycle of native plugins.
 *
 * Typical usage:
 * @code
 *   pvpgn_plugin_context_t ctx = build_context();
 *   PluginLoader loader(ctx);
 *   loader.load_directory("/var/pvpgn/plugins/native");
 *   // ... server runs ...
 *   loader.shutdown_all();
 * @endcode
 */
class PluginLoader {
public:
    /**
     * @brief Construct a loader with the given server context.
     * @param ctx  Context that will be passed to every plugin's init function.
     *             The caller must ensure the callbacks remain valid for the
     *             lifetime of this PluginLoader.
     */
    explicit PluginLoader(pvpgn_plugin_context_t ctx);

    /**
     * @brief Destructor — calls shutdown_all() if not already called.
     */
    ~PluginLoader();

    // Non-copyable, movable.
    PluginLoader(const PluginLoader&)            = delete;
    PluginLoader& operator=(const PluginLoader&) = delete;
    PluginLoader(PluginLoader&&)                 = default;
    PluginLoader& operator=(PluginLoader&&)      = default;

    /**
     * @brief Scan @p dir and load every .so / .dll found there.
     *
     * Files that fail version checks or whose init() returns non-zero are
     * logged and skipped; the remaining plugins are still loaded.
     *
     * @param dir  Directory to scan.  Non-existent directories are silently
     *             ignored.
     */
    void load_directory(const std::filesystem::path& dir);

    /**
     * @brief Load a single plugin shared library.
     *
     * @param path  Full path to the .so / .dll file.
     * @return true if the plugin was loaded and initialised successfully.
     */
    [[nodiscard]] bool load(const std::filesystem::path& path);

    /**
     * @brief Call pvpgn_plugin_shutdown() on every loaded plugin and unload
     *        the shared libraries.  Safe to call multiple times.
     */
    void shutdown_all();

    /**
     * @brief Read-only view of all successfully loaded plugins.
     */
    [[nodiscard]] const std::vector<LoadedPlugin>& loaded_plugins() const noexcept;

private:
    pvpgn_plugin_context_t ctx_;
    std::vector<LoadedPlugin> plugins_;
    bool shut_down_{false};
};

} // namespace pvpgn::infra::plugin
