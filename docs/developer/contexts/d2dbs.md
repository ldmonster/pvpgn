# D2DBS Context — Diablo II Database Server

The **d2dbs** bounded context owns the Diablo II Database Server protocol. It is responsible for
persisting and serving Diablo II character save files — the binary `.d2s` blobs that encode a
character's inventory, skills, quests, and waypoints.

---

## Responsibilities

- Receiving and storing character save files uploaded by the game server (d2gs)
- Serving character save files to d2gs when a character enters a game
- Enforcing save-file integrity (checksum validation, size limits)
- Providing a backup/restore mechanism for character data
- Coordinating with d2cs to ensure a character is not in two games simultaneously

## Key Aggregates and Entities

| Type | Location | Description |
|------|----------|-------------|
| `D2SaveFile` | `src/domain/d2dbs/` | Aggregate root — raw `.d2s` blob + metadata |
| `D2SaveFileId` | `src/domain/d2dbs/` | Strong typedef: account ID + character name |
| `D2SaveChecksum` | `src/domain/d2dbs/` | Value object — CRC32 of the save blob |
| `D2SaveVersion` | `src/domain/d2dbs/` | Enum of known `.d2s` format versions |

## Port Interfaces (`src/domain/d2dbs/ports.hpp`)

| Interface | Purpose |
|-----------|---------|
| `ID2SaveFileRepository` | Store and retrieve `D2SaveFile` blobs |
| `ID2SaveFileValidator` | Validate checksum and structural integrity of a save blob |
| `ID2CharacterLockRegistry` | Prevent concurrent access to the same character save |

## Key Use Cases (`src/application/d2dbs/`)

| Use Case | Trigger | Description |
|----------|---------|-------------|
| `UploadSaveFileUseCase` | d2gs sends save after game ends | Validates checksum, stores via `ID2SaveFileRepository` |
| `DownloadSaveFileUseCase` | d2gs requests save before game starts | Retrieves blob, acquires lock via `ID2CharacterLockRegistry` |
| `ReleaseSaveLockUseCase` | d2gs signals game over | Releases the character lock |
| `BackupSaveFileUseCase` | Scheduled / on upload | Creates a timestamped backup copy |

## Save File Storage

By default, save files are stored as flat files under `${data_dir}/save/`. The `ID2SaveFileRepository`
interface allows alternative backends (e.g., a database BLOB column) to be plugged in without
changing the use cases.

The storage path pattern is:
```
${data_dir}/save/<realm_name>/<account_name>/<character_name>.d2s
```

## Where to Add New Features

- **New save format version** → extend `D2SaveVersion` enum; update `ID2SaveFileValidator`
- **Cloud save storage** → implement `ID2SaveFileRepository` backed by S3/GCS; inject at startup
- **Save file metrics** → emit `d2dbs.save.upload_bytes` and `d2dbs.save.download_bytes` via
  `core::IMetricsRegistry` in the upload/download use cases
- **Corruption detection** → enhance `ID2SaveFileValidator` with structural parsing beyond checksum

## Migration Note

The legacy implementation lives in `src/integration/legacy_d2dbs/`. Plan 04 (d2cs/d2dbs Strangler)
will migrate all logic into `src/application/d2dbs/` and route storage through `infra/persistence/`
(Plan 07). Until then, use cases delegate to legacy code via bridge symbols.

## Related ADRs

- [ADR 0004 — Strangler Fig Pattern](../../adr/0004-strangler-fig-pattern.md)

## See Also

- [D2CS Context](d2cs.md) — character server that coordinates with d2dbs
- [Realm Context](realm.md) — realm and game server registry
