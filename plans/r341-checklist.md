# R341 — Unit Test Backfill for Domain Value Objects

## Checklist
- [x] Read `src/v3/domain/` directory listing — identified value objects: `AccountId`, `ChannelId`, `GameId`, `ClanId`, `TeamId`, `SessionId`, `ConnectionId` (ids.hpp), `UserName`, `IpAddress`, `BNHash`, `ChannelName`, `RealmName`, `ClanTag`, `GameName`
- [x] Read `tests/unit/domain/shared/` — existing tests cover `ConnectionId` (in `value_objects_test.cpp`), `ChannelName`, `RealmName`, `ClanTag`, `GameName` (in `value_objects_test.cpp`), `ClientTag` (in `client_tag_test.cpp`)
- [x] Identified missing dedicated test files: `UserName`, `IpAddress`, `BNHash`, and all strong IDs (`AccountId`, `ChannelId`, `GameId`, `ClanId`, `TeamId`, `SessionId`)
- [x] Created `tests/unit/domain/shared/user_name_test.cpp` — tests: valid construction (min/max length, allowed chars), rejection (empty, too short, too long, starts with digit/underscore, space, control char, at-sign), case-insensitive equality, `display()` vs `canonical()`, `std::hash` consistency
- [x] Created `tests/unit/domain/shared/ip_address_test.cpp` — tests: valid IPv4 parse, family detection, rejection (empty, out-of-range octet, too few/many octets, non-numeric), `v4_packed()` values, `to_string()` round-trip, equality, default construction, direct V4 array construction
- [x] Created `tests/unit/domain/shared/bn_hash_test.cpp` — tests: default construction (all-zero), Bytes array construction, `from_bytes` (valid 20 bytes, reject 19/21/0 bytes), equality (equal/unequal/last-byte-differs/default), `equals_constant_time`, `kSize` constant
- [x] Created `tests/unit/domain/shared/account_id_test.cpp` — tests: `AccountId`, `ChannelId`, `GameId`, `ClanId`, `TeamId`, `SessionId`, `ConnectionId` — construction, `value()`, `is_valid()` (zero invalid), value-based equality/ordering, type safety (distinct types), `std::hash` consistency
- [x] Updated `tests/unit/domain/shared/CMakeLists.txt` — added `pvpgn_v3_add_test` entries for `test_domain_shared_user_name`, `test_domain_shared_ip_address`, `test_domain_shared_bn_hash`, `test_domain_shared_account_id`

## Result
Four new test files provide complete unit test coverage for the domain value objects
that were previously untested or only incidentally exercised:
- `UserName` — 12 test cases covering validation, equality, casing, and hashing
- `IpAddress` — 14 test cases covering IPv4 parsing, packed values, round-trips, and equality
- `BNHash` — 11 test cases covering construction, factory, equality, and constant-time comparison
- Strong IDs (`AccountId` et al.) — 14 test cases covering all 7 ID types, validity, ordering, type safety, and hashing
