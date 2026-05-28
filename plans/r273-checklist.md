# R273 — Replace AdapterRegistry Static State with Instance-Based Factory

## Status: COMPLETE

## Assessment
**Case A** — `AdapterRegistry` existed with static/global state.

The class in `src/v3/infra/persistence/include/infra/persistence/adapter_registry.hpp`
used an all-static API backed by two function-local statics:
- `static std::map<BackendType, FactoryFunction>& registry()` — static map
- `static std::mutex& registry_mutex()` — static mutex

All four methods (`register_backend`, `create`, `available_backends`, `clear`) were
`static`, making the registry a process-wide singleton with no injection point.

`backend_registration.cpp` called `AdapterRegistry::register_backend(...)` directly
(static call). The `register_backends()` free function was never called from outside
`infra/persistence/` — no call sites in `app/`, `runtime/`, or tests.

`IUnitOfWorkFactory` already exists as the canonical port interface
(`src/v3/application/ports/include/application/ports/unit_of_work_factory.hpp`).

## Files Modified

| File | Change |
|------|--------|
| `src/v3/infra/persistence/include/infra/persistence/adapter_registry.hpp` | Replaced `AdapterRegistry` (all-static class) with `AdapterFactory` (instance-based class; `registry_` and `mutex_` are non-static members) |
| `src/v3/infra/persistence/src/adapter_registry.cpp` | Reimplemented all methods as instance methods; removed function-local statics |
| `src/v3/infra/persistence/src/backend_registration.cpp` | Changed `register_backends()` signature from `void register_backends()` to `void register_backends(AdapterFactory& factory)`; replaced static calls with instance calls |

`CMakeLists.txt` required no changes — no new source files were added.

## Notes
- `AdapterFactory` is non-copyable but movable, suitable for injection via
  `std::unique_ptr<AdapterFactory>` or direct ownership at the composition root.
- `IUnitOfWorkFactory` remains the canonical port interface; `AdapterFactory` is an
  infrastructure-layer helper that produces `IUnitOfWorkFactory` instances.
- No call sites outside `infra/persistence/` existed, so no further propagation was
  needed. When `register_backends()` is eventually wired into the composition root
  (e.g., `app/bnetd/src/main.cpp`), it must pass an `AdapterFactory` instance.
- The old `AdapterRegistry` name is fully removed; no deprecated alias was left.
