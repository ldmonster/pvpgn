# R216f -- Wire pure-v3 FileHelpResponder into the chat bridge

Status: **GREEN** (`pvpgn-v3-test:r216f`)

## Scope

Retire `LegacyHelpResponder` from the runtime path. The bridge now
routes `/help` through `FileHelpResponder` -- a pure-v3 implementation
built on the parser introduced in R216e and three new ports.

The legacy `src/bnetd/helpfile.cpp` is still compiled (it is still
called from `main.cpp::helpfile_init` at startup), but its
`handle_help_command` is no longer reached for chat-side `/help`
under the v3 strangler path.

## New ports (application layer)

- [src/v3/application/admin_commands/include/application/admin_commands/help_command_permissions.hpp](src/v3/application/admin_commands/include/application/admin_commands/help_command_permissions.hpp)
  -- `IHelpCommandPermissions::is_visible(conn, canonical_name)`.
- [src/v3/application/admin_commands/include/application/admin_commands/message_sink.hpp](src/v3/application/admin_commands/include/application/admin_commands/message_sink.hpp)
  -- `IMessageSink::send(conn, severity, text)` with
  `Severity::Info | Error`.
- [src/v3/application/admin_commands/include/application/admin_commands/help_corpus_provider.hpp](src/v3/application/admin_commands/include/application/admin_commands/help_corpus_provider.hpp)
  -- `IHelpCorpusProvider::for_connection(conn) -> const HelpCorpus*`.

## New responder

- [src/v3/application/admin_commands/include/application/admin_commands/file_help_responder.hpp](src/v3/application/admin_commands/include/application/admin_commands/file_help_responder.hpp)
- [src/v3/application/admin_commands/src/file_help_responder.cpp](src/v3/application/admin_commands/src/file_help_responder.cpp)
  -- Behaviour mirrors legacy `handle_help_command`:
    * No arg -> "Chat commands :" header, then one space-prefixed line
      per `HelpEntry` whose canonical alias passes the permission
      check; aliases joined with " ".
    * Arg present -> `HelpCorpus::find_by_alias`; on hit, send each
      description line (lines starting with `/` -> `Error`, everything
      else -> `Info`); on miss, send "No help available for that
      command" (Error) and return false.
    * Null corpus -> "There is a problem with the help file" (Error),
      return false (bridge will fall back to legacy dispatch).

## New legacy adapters (`integration_legacy_bnetd_linked`)

- [src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/legacy_help_command_permissions.hpp](src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/legacy_help_command_permissions.hpp)
- [src/v3/integration/legacy_bnetd/src/legacy_help_command_permissions.cpp](src/v3/integration/legacy_bnetd/src/legacy_help_command_permissions.cpp)
  -- `command_get_group(name) & account_get_command_groups(conn_get_account(c)) != 0`.
- [src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/legacy_message_sink.hpp](src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/legacy_message_sink.hpp)
- [src/v3/integration/legacy_bnetd/src/legacy_message_sink.cpp](src/v3/integration/legacy_bnetd/src/legacy_message_sink.cpp)
  -- `message_send_text(c, message_type_info|message_type_error, c, text)`.
- [src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/legacy_help_corpus_provider.hpp](src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/legacy_help_corpus_provider.hpp)
- [src/v3/integration/legacy_bnetd/src/legacy_help_corpus_provider.cpp](src/v3/integration/legacy_bnetd/src/legacy_help_corpus_provider.cpp)
  -- `std::call_once` first-call init that walks the legacy
  `languages` vector, builds each localized path via
  `i18n_filename(prefs_v3::helpfile(), lang.gamelang)`, parses with
  `parse_help_corpus`, caches keyed on `unsigned`-cast gamelang.
  `for_connection` looks up by `conn_get_gamelang_localized(c)` and
  falls back to the first loaded locale (matching legacy `get_hfd`).

## Bridge change

- [src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp](src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp):
  the `/help` branch now constructs four function-local statics
  (`LegacyHelpCorpusProvider`, `LegacyHelpCommandPermissions`,
  `LegacyMessageSink`, and the `FileHelpResponder` wired to them)
  and calls `respond`. `LegacyHelpResponder` is no longer used at
  runtime; the include is kept temporarily for a one-round overlap
  before it gets deleted in a follow-up.

## Behaviour deviations from legacy (documented)

- Legacy `list_commands` permission filter uses the LAST alias's
  group check (a copy-paste bug in `helpfile.cpp`). The v3 path uses
  the FIRST (canonical) alias instead. Practically equivalent for
  all in-tree help entries.
- Legacy uses `i18n_convert(c, line)` per body line to localize
  presentation. R216f does not pipe body lines through that
  converter -- the description lines come from the localized file
  for the connection's gamelang directly, which already matches the
  language. Edge cases involving runtime gamelang switches between
  init and dispatch are not handled.

## Tests

- [tests/unit/application/admin_commands/file_help_responder_test.cpp](tests/unit/application/admin_commands/file_help_responder_test.cpp)
  -- 8 cases using fake `IHelpCorpusProvider`, `IHelpCommandPermissions`,
  `IMessageSink`. Covers list-mode header + per-entry filtering,
  describe-mode hit/miss/severity, null-corpus error path, deny-all.

Test binary `test_application_admin_commands_file_help_responder`
added to Dockerfile build and run lists.

## What this round does NOT do

- Does not delete `LegacyHelpResponder` or `legacy_help_responder.cpp`
  (kept for one-round overlap; pending removal in R216g).
- Does not remove `helpfile_init` / `handle_help_command` from
  `src/bnetd/helpfile.cpp` -- they are still called by `main.cpp` at
  startup. Removing them requires also removing the startup call
  and the prefs key; deferred to a separate clean-up round.

## Verify

```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r216f .
```

Success marker: `naming to docker.io/library/pvpgn-v3-test:r216f done`.
