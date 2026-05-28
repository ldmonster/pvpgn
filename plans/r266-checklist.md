# R266 — Missing Port Headers (Persistence + Scripting)

## Status: ✅ COMPLETE

### New port headers created
- [x] channel_store.hpp — IChannelStore (load_all, save, remove)
- [x] tournament_repository.hpp — ITournamentRepository (find_by_id, list_active, get_standings, record_result)
- [x] script_host.hpp — IScriptHost (load_file, unload, call, has_function)
- [x] script_sandbox.hpp — IScriptSandbox (set_limits, limits, reset_limits)

### Build
- [x] ports/CMakeLists.txt updated
