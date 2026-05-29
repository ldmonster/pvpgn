// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file api.h
 * @brief PvPGN Plugin C ABI 1.0 — stable interface for native plugins.
 *
 * This header defines the stable C ABI that native plugins (.so/.dll) must
 * implement. It is intentionally C99-compatible so that plugins can be written
 * in plain C without requiring a C++ toolchain.
 *
 * Every plugin shared library must export exactly three symbols:
 *   - pvpgn_plugin_get_info()  — returns static metadata
 *   - pvpgn_plugin_init()      — called once on load; returns 0 on success
 *   - pvpgn_plugin_shutdown()  — called once on unload
 *
 * The server passes a @ref pvpgn_plugin_context_t to pvpgn_plugin_init(),
 * which provides callbacks the plugin may use to log messages and emit events.
 *
 * @par ABI stability guarantee
 * Fields may only be appended to the end of structs. The @c api_version field
 * allows the loader to detect version mismatches at runtime.
 */
#ifndef PVPGN_PLUGIN_API_H
#define PVPGN_PLUGIN_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Current plugin ABI version. Increment when the ABI changes incompatibly. */
#define PVPGN_PLUGIN_API_VERSION 1

/**
 * @brief Server-provided context passed to pvpgn_plugin_init().
 *
 * The plugin must not store a pointer to this struct beyond the lifetime of
 * the init call; instead it should copy the individual function pointers and
 * the @c server_handle it needs.
 */
typedef struct pvpgn_plugin_context {
    /** ABI version of the context struct (equals PVPGN_PLUGIN_API_VERSION). */
    uint32_t api_version;

    /** Opaque handle that must be passed back to every server callback. */
    void* server_handle;

    /**
     * @brief Log a message through the server's logging subsystem.
     * @param server_handle  Opaque handle from this struct.
     * @param level          Severity: 0=debug, 1=info, 2=warn, 3=error.
     * @param message        NUL-terminated UTF-8 message string.
     */
    void (*log)(void* server_handle, int level, const char* message);

    /**
     * @brief Emit a named event with a JSON payload to the server event bus.
     * @param server_handle  Opaque handle from this struct.
     * @param event_name     NUL-terminated event name (e.g. "user.joined").
     * @param payload_json   NUL-terminated JSON string (may be "{}" but not NULL).
     */
    void (*emit_event)(void* server_handle, const char* event_name, const char* payload_json);
} pvpgn_plugin_context_t;

/**
 * @brief Static metadata returned by pvpgn_plugin_get_info().
 *
 * All string pointers must point to static storage (string literals) that
 * remains valid for the lifetime of the loaded shared library.
 */
typedef struct pvpgn_plugin_info {
    /** Must equal PVPGN_PLUGIN_API_VERSION; checked by the loader. */
    uint32_t api_version;

    /** Short plugin identifier, e.g. "example-quiz". No spaces. */
    const char* name;

    /** Semantic version string, e.g. "1.0.0". */
    const char* version;

    /** Human-readable description of the plugin. */
    const char* description;

    /** Author name or email. */
    const char* author;
} pvpgn_plugin_info_t;

/* -------------------------------------------------------------------------
 * Function pointer typedefs for the three mandatory plugin entry points.
 * ------------------------------------------------------------------------- */

/** Signature of pvpgn_plugin_get_info(). */
typedef const pvpgn_plugin_info_t* (*pvpgn_plugin_get_info_fn)(void);

/**
 * Signature of pvpgn_plugin_init().
 * @return 0 on success; non-zero causes the loader to unload the plugin.
 */
typedef int (*pvpgn_plugin_init_fn)(pvpgn_plugin_context_t* ctx);

/** Signature of pvpgn_plugin_shutdown(). */
typedef void (*pvpgn_plugin_shutdown_fn)(void);

/* -------------------------------------------------------------------------
 * Convenience macro for implementing pvpgn_plugin_get_info().
 * Usage:
 *   static const pvpgn_plugin_info_t my_info = { ... };
 *   PVPGN_PLUGIN_EXPORT_INFO(&my_info)
 * ------------------------------------------------------------------------- */
#define PVPGN_PLUGIN_EXPORT_INFO(info_ptr) \
    const pvpgn_plugin_info_t* pvpgn_plugin_get_info(void) { return (info_ptr); }

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PVPGN_PLUGIN_API_H */
