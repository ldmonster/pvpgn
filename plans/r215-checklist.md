# R215 — `enum class` sweep across v3 domain + application

Status: **GREEN ✓ (no-op)** — codebase already conformant.

## Audit
Pattern `^\s*enum\s+(?!class\b|struct\b)[A-Za-z_]\w*\s*[:{]` across `src/v3/**`
returned **zero matches**. Every enumerated type declared inside the v3 tree
is already `enum class`, including:

- `core::StatusCode`, `core::LogLevel`
- `domain::chat::ChannelFlag`, `JoinOutcome`, `WhisperOutcome`
- `domain::social::AddOutcome`, `ClanRank`, `JoinOutcome`
- `domain::d2cs::CharacterClass`, `CharacterFlags`
- `domain::d2dbs::CharacterLockState`
- `protocol::wolgameres::DataType`
- `integration::wol::State`, `WolPacketType`
- `runtime::HealthStatus`, `runtime::service_logger::LogLevel`
- v3 tools: `bnclient_proto::PacketClass`, `bnftp_v3::ExistsAction`,
  `bnbot_v3::LineState`

Only unscoped enums in v3 are inside `src/v3/tools/bniutils/tga.h` (TGA image
format constants in a legacy converter tool — out of scope for this round).

## Conclusion
No code changes required. R215 is satisfied by prior development hygiene.
Future enforcement is delegated to compiler flags (`-Werror` + `-Wpedantic`
already in `pvpgn_v3_apply_flags`) plus reviewer vigilance; no dedicated
lint pass needed.
