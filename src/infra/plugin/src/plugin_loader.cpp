// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file plugin_loader.cpp
 * @brief Implementation of PluginLoader — loads native plugins via dlopen/LoadLibrary.
 */

#include "infra/plugin/plugin_loader.hpp"

#include <cstring>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <string>

// ---- Platform-specific dynamic-library API ----------------------------------
#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
   using lib_handle_t = HMODULE;
   static lib_handle_t lib_open(const char* path)  { return ::LoadLibraryA(path); }
   static void*        lib_sym(lib_handle_t h, const char* sym) {
       return reinterpret_cast<void*>(::GetProcAddress(h, sym));
   }
   static void         lib_close(lib_handle_t h)   { ::FreeLibrary(h); }
   static std::string  lib_error()                 {
       DWORD e = ::GetLastError();
       char buf[256] = {};
       ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, e, 0, buf, sizeof(buf), nullptr);
       return buf;
   }
   static constexpr const char* k_lib_ext = ".dll";
#else
#  include <dlfcn.h>
   using lib_handle_t = void*;
   static lib_handle_t lib_open(const char* path)  { return ::dlopen(path, RTLD_NOW | RTLD_LOCAL); }
   static void*        lib_sym(lib_handle_t h, const char* sym) { return ::dlsym(h, sym); }
   static void         lib_close(lib_handle_t h)   { ::dlclose(h); }
   static std::string  lib_error()                 {
       const char* e = ::dlerror();
       return e ? e : "(unknown error)";
   }
#  if defined(__APPLE__)
   static constexpr const char* k_lib_ext = ".dylib";
#  else
   static constexpr const char* k_lib_ext = ".so";
#  endif
#endif
// -----------------------------------------------------------------------------

namespace pvpgn::infra::plugin {

namespace {

/// Helper: call the context's log callback (level 1 = info, 3 = error).
void ctx_log(const pvpgn_plugin_context_t& ctx, int level, const std::string& msg) {
    if (ctx.log) {
        ctx.log(ctx.server_handle, level, msg.c_str());
    }
}

} // anonymous namespace

// ---- PluginLoader -----------------------------------------------------------

PluginLoader::PluginLoader(pvpgn_plugin_context_t ctx)
    : ctx_(ctx)
{}

PluginLoader::~PluginLoader() {
    shutdown_all();
}

void PluginLoader::load_directory(const std::filesystem::path& dir) {
    if (!std::filesystem::exists(dir)) {
        ctx_log(ctx_, 1, std::format("PluginLoader: directory '{}' does not exist, skipping.",
                                     dir.string()));
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != k_lib_ext) continue;
        load(entry.path());
    }
}

bool PluginLoader::load(const std::filesystem::path& path) {
    const std::string path_str = path.string();

    // Open the shared library.
    lib_handle_t handle = lib_open(path_str.c_str());
    if (!handle) {
        ctx_log(ctx_, 3, std::format("PluginLoader: failed to open '{}': {}",
                                     path_str, lib_error()));
        return false;
    }

    // Resolve mandatory symbols.
    auto get_info_fn = reinterpret_cast<pvpgn_plugin_get_info_fn>(
        lib_sym(handle, "pvpgn_plugin_get_info"));
    auto init_fn = reinterpret_cast<pvpgn_plugin_init_fn>(
        lib_sym(handle, "pvpgn_plugin_init"));
    auto shutdown_fn = reinterpret_cast<pvpgn_plugin_shutdown_fn>(
        lib_sym(handle, "pvpgn_plugin_shutdown"));

    if (!get_info_fn || !init_fn || !shutdown_fn) {
        ctx_log(ctx_, 3, std::format(
            "PluginLoader: '{}' is missing required symbols "
            "(pvpgn_plugin_get_info / pvpgn_plugin_init / pvpgn_plugin_shutdown).",
            path_str));
        lib_close(handle);
        return false;
    }

    // Retrieve plugin metadata.
    const pvpgn_plugin_info_t* info = get_info_fn();
    if (!info) {
        ctx_log(ctx_, 3, std::format("PluginLoader: '{}' returned NULL from pvpgn_plugin_get_info.",
                                     path_str));
        lib_close(handle);
        return false;
    }

    // Check ABI version.
    if (info->api_version != PVPGN_PLUGIN_API_VERSION) {
        ctx_log(ctx_, 3, std::format(
            "PluginLoader: '{}' has api_version={} but server requires {}; skipping.",
            path_str, info->api_version, PVPGN_PLUGIN_API_VERSION));
        lib_close(handle);
        return false;
    }

    const std::string plugin_name    = info->name    ? info->name    : "(unnamed)";
    const std::string plugin_version = info->version ? info->version : "0.0.0";

    ctx_log(ctx_, 1, std::format("PluginLoader: loading plugin '{}' v{} from '{}'.",
                                  plugin_name, plugin_version, path_str));

    // Initialise the plugin.
    int rc = init_fn(&ctx_);
    if (rc != 0) {
        ctx_log(ctx_, 3, std::format(
            "PluginLoader: plugin '{}' init() returned {} (non-zero); unloading.",
            plugin_name, rc));
        lib_close(handle);
        return false;
    }

    ctx_log(ctx_, 1, std::format("PluginLoader: plugin '{}' v{} loaded successfully.",
                                  plugin_name, plugin_version));

    plugins_.push_back(LoadedPlugin{
        .name     = plugin_name,
        .version  = plugin_version,
        .handle   = static_cast<void*>(handle),
        .shutdown = shutdown_fn,
    });

    return true;
}

void PluginLoader::shutdown_all() {
    if (shut_down_) return;
    shut_down_ = true;

    // Shutdown in reverse load order.
    for (auto it = plugins_.rbegin(); it != plugins_.rend(); ++it) {
        ctx_log(ctx_, 1, std::format("PluginLoader: shutting down plugin '{}'.", it->name));
        if (it->shutdown) {
            it->shutdown();
        }
        if (it->handle) {
            lib_close(static_cast<lib_handle_t>(it->handle));
            it->handle = nullptr;
        }
    }
    plugins_.clear();
}

const std::vector<LoadedPlugin>& PluginLoader::loaded_plugins() const noexcept {
    return plugins_;
}

} // namespace pvpgn::infra::plugin
