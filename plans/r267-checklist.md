# R267 — I/O Port Headers + Null Adapters

## Status: ✅ COMPLETE

### New port headers
- [x] event_loop.hpp — IEventLoop (run, stop, post, is_running)
- [x] listener.hpp — IListener (listen, close, local_port)
- [x] connection.hpp — IConnection (write, on_read, on_error, close, is_open, remote_ip, remote_port)
- [x] resolver.hpp — IResolver (resolve, reverse_lookup)

### New null adapters
- [x] null_event_loop.hpp — NullEventLoop (synchronous, for tests)
- [x] null_resolver.hpp — NullResolver (returns fixed address, for tests)

### Build
- [x] ports/CMakeLists.txt updated
- [x] infra/inmemory/CMakeLists.txt updated
