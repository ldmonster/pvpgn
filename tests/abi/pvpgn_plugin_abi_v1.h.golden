/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file pvpgn/plugin/abi.h
 * @brief PvPGN public plugin ABI — versioned, stable C interface (Plan 12).
 *
 * This is the ONE public, installed header a native plugin compiles against.
 * It is pure C99 (no C++), exposes NO `domain/`/`application/` C++ symbols, and
 * is the semver contract for the plugin boundary: a breaking change requires a
 * new `pvpgn/plugin/abi_v2.h` and a deprecation cycle for v1 (enforced by
 * `scripts/dev/check-plugin-abi.sh`).
 *
 * ## ABI version
 * The integer `PVPGN_PLUGIN_ABI_VERSION` identifies this contract. Within a
 * version, structs may only GROW by APPENDING fields; existing fields never
 * change type or order. The loader checks `api_version` at load time.
 *
 * ## A plugin shared library exports
 *   - `pvpgn_plugin_get_info`          (required) — static metadata
 *   - `pvpgn_plugin_init`              (required) — called once on load
 *   - `pvpgn_plugin_shutdown`          (required) — called once on unload
 *   - `pvpgn_plugin_get_capabilities`  (optional) — required capability bitmask
 *
 * ## Capabilities
 * A plugin declares the capabilities it needs both in its `.toml` manifest
 * (authoritative, human-auditable) and, optionally, via
 * `pvpgn_plugin_get_capabilities()`. The host grants the manifest set, verifies
 * the exported bitmask is a SUBSET of it at load time, and enforces each
 * capability at the call boundary (O(1) bitmask test). The `PVPGN_CAP_*` bit
 * values below match the host's internal capability enum exactly.
 */
#ifndef PVPGN_PLUGIN_ABI_H
#define PVPGN_PLUGIN_ABI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The plugin ABI version this header defines. */
#define PVPGN_PLUGIN_ABI_VERSION 1

/* -------------------------------------------------------------------------
 * Capability tokens — bitmask flags. Bit values are part of the ABI and match
 * the host's internal `pvpgn::infra::scripting::plugin::Capability` enum.
 * ------------------------------------------------------------------------- */
typedef uint32_t pvpgn_capabilities_t;

#define PVPGN_CAP_CHAT_SEND          (1u << 0)  /* send to channels / whisper   */
#define PVPGN_CAP_CHAT_EMOTE         (1u << 1)  /* emote                        */
#define PVPGN_CAP_DB_READ            (1u << 2)  /* read account/store data      */
#define PVPGN_CAP_DB_WRITE           (1u << 3)  /* mutate account/store data    */
#define PVPGN_CAP_EVENTS_SUBSCRIBE   (1u << 4)  /* subscribe to server events   */
#define PVPGN_CAP_EVENTS_PUBLISH     (1u << 5)  /* publish events               */
#define PVPGN_CAP_FS_READ            (1u << 6)  /* read files                   */
#define PVPGN_CAP_FS_WRITE           (1u << 7)  /* write files                  */
#define PVPGN_CAP_NET_HTTP           (1u << 8)  /* outbound HTTP                */
#define PVPGN_CAP_NET_SOCKET         (1u << 9)  /* raw sockets                  */
#define PVPGN_CAP_COMMANDS_REGISTER  (1u << 10) /* register chat commands       */
#define PVPGN_CAP_MODERATION_BAN     (1u << 11) /* ban accounts                 */
#define PVPGN_CAP_MODERATION_KICK    (1u << 12) /* kick connections             */
#define PVPGN_CAP_STORE_READ         (1u << 13) /* read the plugin KV store     */
#define PVPGN_CAP_STORE_WRITE        (1u << 14) /* write the plugin KV store    */
#define PVPGN_CAP_ADMIN_RELOAD_CONFIG (1u << 15)/* reload server config         */
#define PVPGN_CAP_ADMIN_SHUTDOWN     (1u << 16) /* shut the server down         */

/* -------------------------------------------------------------------------
 * Host context — passed to pvpgn_plugin_init(). The plugin copies the function
 * pointers / handle it needs; it must NOT retain the struct pointer past init.
 * ------------------------------------------------------------------------- */
typedef struct pvpgn_plugin_context {
    /** ABI version of this context (equals PVPGN_PLUGIN_ABI_VERSION). */
    uint32_t api_version;

    /** Opaque host handle; pass back to every host callback. */
    void* server_handle;

    /**
     * Log a message. @p level: 0=debug, 1=info, 2=warn, 3=error.
     * @p message is a NUL-terminated UTF-8 string.
     */
    void (*log)(void* server_handle, int level, const char* message);

    /**
     * Emit a named event with a JSON payload to the host event bus.
     * Requires the PVPGN_CAP_EVENTS_PUBLISH capability. @p payload_json may be
     * "{}" but never NULL.
     */
    void (*emit_event)(void* server_handle, const char* event_name,
                       const char* payload_json);
} pvpgn_plugin_context_t;

/* -------------------------------------------------------------------------
 * Static plugin metadata — returned by pvpgn_plugin_get_info(). All string
 * pointers must reference static storage valid for the library's lifetime.
 * ------------------------------------------------------------------------- */
typedef struct pvpgn_plugin_info {
    /** Must equal PVPGN_PLUGIN_ABI_VERSION; checked by the loader. */
    uint32_t api_version;
    const char* name;        /**< short id, no spaces (e.g. "example-quiz") */
    const char* version;     /**< semver, e.g. "1.0.0"                      */
    const char* description; /**< human-readable description                */
    const char* author;      /**< author name or email                     */
} pvpgn_plugin_info_t;

/* -------------------------------------------------------------------------
 * Entry-point signatures.
 * ------------------------------------------------------------------------- */

/** `const pvpgn_plugin_info_t* pvpgn_plugin_get_info(void)`. */
typedef const pvpgn_plugin_info_t* (*pvpgn_plugin_get_info_fn)(void);

/** `int pvpgn_plugin_init(pvpgn_plugin_context_t*)` — 0 = ok, non-zero unloads. */
typedef int (*pvpgn_plugin_init_fn)(pvpgn_plugin_context_t* ctx);

/** `void pvpgn_plugin_shutdown(void)`. */
typedef void (*pvpgn_plugin_shutdown_fn)(void);

/**
 * Optional: `pvpgn_capabilities_t pvpgn_plugin_get_capabilities(void)` — the
 * OR of PVPGN_CAP_* this plugin needs. Absent export ⇒ 0 (no capabilities).
 * The loader verifies the returned set is a subset of the manifest grant.
 */
typedef pvpgn_capabilities_t (*pvpgn_plugin_get_capabilities_fn)(void);

/* -------------------------------------------------------------------------
 * Convenience macro for implementing pvpgn_plugin_get_info().
 * ------------------------------------------------------------------------- */
#define PVPGN_PLUGIN_EXPORT_INFO(info_ptr) \
    const pvpgn_plugin_info_t* pvpgn_plugin_get_info(void) { return (info_ptr); }

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PVPGN_PLUGIN_ABI_H */
