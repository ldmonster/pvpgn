# R260 — chat: SendWhisper + OpFromChannel

## Status: ✅ COMPLETE

### Use cases
- [x] send_whisper.hpp — SendWhisper (port-injected, `std::shared_ptr` injection)
- [x] send_whisper.cpp — implementation (validates message, resolves target, checks session)
- [x] op_from_channel.hpp — OpFromChannel (port-injected)
- [x] op_from_channel.cpp — implementation (checks operator permission, resolves target, verifies membership)

### Tests
- [x] send_whisper_test.cpp — 4 test cases (happy path, target not found, target offline, empty message)
- [x] op_from_channel_test.cpp — 4 test cases (grant op, revoke op, permission denied, target not in channel)

### Build
- [x] chat/CMakeLists.txt updated (added send_whisper.cpp, op_from_channel.cpp)
- [x] tests/unit/application/chat/CMakeLists.txt updated (added send_whisper and op_from_channel test targets)
