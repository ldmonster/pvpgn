# R216e -- Pure-v3 help corpus model + parser

Status: **GREEN** (`pvpgn-v3-test:r216e`)

## Scope

Introduce a pure-v3 model of the chat `/help` corpus
(`HelpCorpus`, `HelpEntry`) plus a stream-based parser
(`parse_help_corpus`) that reproduces the legacy `helpfile.cpp`
reader's behaviour line-for-line.

No runtime wiring change: the bridge still uses
`LegacyHelpResponder`. R216f will wire a `FileHelpResponder` (pure
v3) on top of these new types and retire the legacy adapter.

## Files added

- [src/v3/application/admin_commands/include/application/admin_commands/help_corpus.hpp](src/v3/application/admin_commands/include/application/admin_commands/help_corpus.hpp)
  -- `HelpEntry` (aliases + description lines) and `HelpCorpus`
  container with case-insensitive `find_by_alias`.
- [src/v3/application/admin_commands/include/application/admin_commands/help_corpus_parser.hpp](src/v3/application/admin_commands/include/application/admin_commands/help_corpus_parser.hpp)
  -- declares `core::Result<HelpCorpus, core::Error> parse_help_corpus(std::istream&)`.
- [src/v3/application/admin_commands/src/help_corpus.cpp](src/v3/application/admin_commands/src/help_corpus.cpp)
  -- `find_by_alias` implementation.
- [src/v3/application/admin_commands/src/help_corpus_parser.cpp](src/v3/application/admin_commands/src/help_corpus_parser.cpp)
  -- parser implementation.
- [tests/unit/application/admin_commands/help_corpus_parser_test.cpp](tests/unit/application/admin_commands/help_corpus_parser_test.cpp)
  -- 17 Catch2 cases covering: empty input, single header, alias
  splitting, trailing-`#` truncation on header and body, full-line
  comments, tab expansion, blank-line dropping, multiple entries,
  pre-header garbage, CRLF endings, leading whitespace, all
  `find_by_alias` paths (canonical / alias / no-slash / case-insensitive
  / miss).

## Files modified

- [src/v3/CMakeLists.txt](src/v3/CMakeLists.txt) -- added
  `help_corpus.cpp` and `help_corpus_parser.cpp` to
  `application_admin_commands` SOURCES.
- [tests/unit/application/admin_commands/CMakeLists.txt](tests/unit/application/admin_commands/CMakeLists.txt)
  -- new test target `test_application_admin_commands_help_corpus_parser`.
- [Dockerfile.v3](Dockerfile.v3) -- added new test binary to both the
  build target list (line ~63) and the run-step list (line ~214).

## Parser semantics (faithful port of legacy reader)

- Lines whose first non-whitespace char is `%` start a new entry.
- The header line is split on whitespace: first token has its `%`
  prefix stripped; every token is then `/`-prefixed.
- Trailing `# ...` on a header line (any position past the `%`) is
  dropped before tokenisation.
- Description lines until the next `%`-line belong to the current
  entry, with: full-line `#` comments dropped, trailing `# ...`
  truncated, tabs expanded to three spaces, all-blank lines dropped.
- Lines before the first `%` and lines with empty `%` headers are
  silently skipped.
- CRLF endings are normalised.

## What this round does NOT do

- Does not introduce a `FileHelpResponder`. Bridge still routes
  `/help` through `LegacyHelpResponder` which calls legacy
  `handle_help_command`.
- Does not load the corpus from disk at server startup. That bootstrap
  step is part of R216f along with the responder, the locale lookup
  port, and the i18n string-table port.
- Does not handle the legacy permission filter
  (`command_get_group` + `account_get_command_groups`) -- that's a
  separate port (`IHelpCommandPermissions`) in R216f.

## Verify

```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r216e .
```

Success marker: `naming to docker.io/library/pvpgn-v3-test:r216e done`.

The new parser test runs as part of the run-step and reports
17 assertions (Catch2 compact reporter -- failures would print
`failed:` lines).
