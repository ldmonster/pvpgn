# R346 Checklist — Stabilise `pvpgn/plugin/api.h` (C ABI 1.0) + Plugin Loader

## Goal
Define a stable C ABI for native plugins (`.so`/`.dll`) and implement a plugin loader.

## Files Created

- [x] `src/v3/infra/plugin/include/infra/plugin/api.h`
  - C99-compatible header with `#ifdef __cplusplus extern "C"` guards
  - `pvpgn_plugin_context_t` — server-provided callbacks (log, emit_event)
  - `pvpgn_plugin_info_t` — static plugin metadata (api_version, name, version, description, author)
  - Function pointer typedefs: `pvpgn_plugin_get_info_fn`, `pvpgn_plugin_init_fn`, `pvpgn_plugin_shutdown_fn`
  - `PVPGN_PLUGIN_EXPORT_INFO(info_ptr)` convenience macro
  - `PVPGN_PLUGIN_API_VERSION = 1`

- [x] `src/v3/infra/plugin/include/infra/plugin/plugin_loader.hpp`
  - `LoadedPlugin` struct: name, version, handle, shutdown fn ptr
  - `PluginLoader` class: constructor takes `pvpgn_plugin_context_t`
  - `load_directory(path)` — scans for `.so`/`.dll` files
  - `load(path)` — loads single plugin, returns bool
  - `shutdown_all()` — reverse-order shutdown + dlclose
  - `loaded_plugins()` — read-only view

- [x] `src/v3/infra/plugin/src/plugin_loader.cpp`
  - POSIX: `dlopen`/`dlsym`/`dlclose` via `<dlfcn.h>`
  - Windows: `LoadLibraryA`/`GetProcAddress`/`FreeLibrary` via `<windows.h>`
  - macOS: `.dylib` extension support
  - ABI version check: logs and skips mismatched plugins
  - `pvpgn_plugin_init` return code check: non-zero → unload + log error
  - `shutdown_all()` calls shutdown fn then dlclose in reverse order
  - Uses `std::format` for log messages

- [x] `src/v3/infra/plugin/CMakeLists.txt`
  - `pvpgn_v3_add_library(pvpgn_infra_plugin STATIC ...)`
  - Links `pvpgn_core` (PUBLIC)
  - Links `${CMAKE_DL_LIBS}` on UNIX
  - `PVPGN_V3_WITH_SECCOMP` option (default OFF) — wires in seccomp for R347
  - `pvpgn_v3_target_warnings(pvpgn_infra_plugin)`

- [x] `plugins/example-quiz/native/main.c`
  - Minimal C99 plugin using the new C ABI
  - Exports `pvpgn_plugin_get_info` via `PVPGN_PLUGIN_EXPORT_INFO` macro
  - `pvpgn_plugin_init`: logs startup message + emits `plugin.started` event
  - `pvpgn_plugin_shutdown`: cleanup stub

- [x] `plugins/example-quiz/native/CMakeLists.txt`
  - Builds `example_quiz_plugin` as a shared library (no `lib` prefix)
  - Includes `src/v3/infra/plugin/include` for the C ABI header
  - C99 standard
  - Install target: `plugins/native/`

## Acceptance Criteria

- [x] `api.h` is C99-compatible and compiles as both C and C++
- [x] `PVPGN_PLUGIN_API_VERSION = 1` defined
- [x] All three mandatory symbols documented and typedef'd
- [x] `PluginLoader` handles POSIX and Windows platforms
- [x] Version mismatch → log + skip (no crash)
- [x] `init()` failure → unload + log (no crash)
- [x] `shutdown_all()` is idempotent (safe to call twice)
- [x] Example native plugin compiles with the new header
