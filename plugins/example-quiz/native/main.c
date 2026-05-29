// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file main.c
 * @brief Example Quiz native plugin — demonstrates the pvpgn C ABI 1.0.
 *
 * Build this file as a shared library and place it in the native plugin
 * directory.  The server will call pvpgn_plugin_get_info() to verify the ABI
 * version, then pvpgn_plugin_init() to start the plugin, and finally
 * pvpgn_plugin_shutdown() when the server shuts down.
 *
 * Compile (Linux example):
 *   gcc -shared -fPIC -o example_quiz_plugin.so main.c \
 *       -I../../../../src/v3/infra/plugin/include
 */

#include "infra/plugin/api.h"

#include <stdio.h>
#include <string.h>

/* ---- Static plugin metadata ----------------------------------------------- */

static const pvpgn_plugin_info_t s_info = {
    .api_version = PVPGN_PLUGIN_API_VERSION,
    .name        = "example-quiz",
    .version     = "1.0.0",
    .description = "Example quiz game plugin (native C ABI demonstration)",
    .author      = "PvPGN Team",
};

/* ---- Mandatory exports ----------------------------------------------------- */

/* Export pvpgn_plugin_get_info using the convenience macro. */
PVPGN_PLUGIN_EXPORT_INFO(&s_info)

/**
 * @brief Plugin initialisation.
 *
 * Called once by the server after the shared library is loaded.
 * Store the context callbacks for later use.
 *
 * @param ctx  Server-provided context; valid only during this call.
 * @return 0 on success; non-zero causes the loader to unload the plugin.
 */
int pvpgn_plugin_init(pvpgn_plugin_context_t* ctx) {
    if (!ctx) return -1;

    /* Log a startup message through the server's logging subsystem. */
    if (ctx->log) {
        ctx->log(ctx->server_handle, 1 /* info */,
                 "example-quiz native plugin: initialised (C ABI 1.0)");
    }

    /* Emit a startup event so other subsystems can react. */
    if (ctx->emit_event) {
        ctx->emit_event(ctx->server_handle,
                        "plugin.started",
                        "{\"plugin\":\"example-quiz\",\"version\":\"1.0.0\"}");
    }

    return 0; /* success */
}

/**
 * @brief Plugin shutdown.
 *
 * Called once by the server before the shared library is unloaded.
 * Release any resources acquired during init.
 */
void pvpgn_plugin_shutdown(void) {
    /* Nothing to clean up in this minimal example. */
    printf("[example-quiz] native plugin: shutdown\n");
}
