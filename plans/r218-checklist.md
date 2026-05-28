# R218 -- Fix layering violations exposed by R217

Status: **GREEN** (`pvpgn-v3-test:r218`)

Bundles three fixes that R217's new layering check surfaced.

## #3 -- `application_ports` had incomplete PUBLIC_DEPS
The live `application_ports` target (defined inline at
`src/v3/CMakeLists.txt:640`, not in `src/v3/application/ports/CMakeLists.txt`
which is dead code) declared only domain_shared/identity/chat/gameplay/
moderation/social. Added `domain_realm` and `domain_ladder` (required by
`realm_repository.hpp` and `ladder_repository.hpp` respectively).

Tests' `test_application_ports_headers_selfcontained` no longer needs
the extra DEPS list -- it now gets them transitively from
`application_ports`. (Left as-is to avoid extra churn.)

## #2 -- `inforeply_builder.cpp` directly included `infra/compression`
Hexagonal port + adapter:

- **New port**: `application/ports/anongame_compressor.hpp` declares
  `class IAnonGameCompressor { virtual Result<vector<uint8_t>> compress(...) const = 0; };`.
- **`inforeply_builder.{hpp,cpp}`**: six public functions now take a
  `const IAnonGameCompressor&` parameter (`compose_inforeply`,
  `build_inforeply_for_tag(snapshot)`, `build_inforeplies_for_request(snapshot)`,
  `encode_inforeplies_for_request(snapshot)`, `compile_snapshot`,
  `compile_snapshot_set`). `infra/compression/zlib_anongame.hpp` is
  no longer included from this TU.
- **New adapter**: `infra/compression/zlib_anongame_compressor.hpp`
  defines `ZlibAnonGameCompressor final : IAnonGameCompressor` that
  delegates to the existing `anongame_compress` free function.
- **`infra_compression` library** now PUBLIC-depends on
  `application_ports` (canonical hexagonal: infra implements ports).
- **`application_anongame_infoply`** no longer depends on
  `infra_compression`; now PUBLIC-depends on `application_ports`.
- **Callers updated**:
  - `tests/unit/application/anongame_infoply/inforeply_builder_test.cpp`
    constructs a `kCompressor` instance once and passes it to every
    snapshot-variant call.
  - `src/v3/integration/legacy_bnetd/src/anongame_bootstrap.cpp`
    constructs the adapter as a function-local `static` and passes
    it to both `compile_snapshot_set` call sites.
  - `integration_legacy_bnetd` library gains `infra_compression` in
    PUBLIC_DEPS so the adapter header resolves.

## Side effect: allow-list shrunk
`scripts/v3_layering_check.sh` allow-list entry for
`inforeply_builder.cpp -> zlib_anongame.hpp` removed. Only the
`config_subscriber.hpp -> server_config.hpp` entry remains (deferred
to a future round).

## Verify
```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r218 .
```
Success marker: `naming to docker.io/library/pvpgn-v3-test:r218 done`.

`v3-layer-check` stage prints only one `ALLOWED (TODO):` line now.
