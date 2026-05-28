# R268 — InMemory Fakes for Missing Ports

## Status: COMPLETE

## Files Created

- `src/v3/infra/inmemory/include/infra/inmemory/in_memory_team_repository.hpp`
- `src/v3/infra/inmemory/include/infra/inmemory/in_memory_mail_store.hpp`
- `src/v3/infra/inmemory/include/infra/inmemory/in_memory_news_store.hpp`
- `src/v3/infra/inmemory/include/infra/inmemory/in_memory_helpfile_source.hpp`
- `src/v3/infra/inmemory/include/infra/inmemory/in_memory_icon_provider.hpp`
- `src/v3/infra/inmemory/include/infra/inmemory/in_memory_random_source.hpp`
- `src/v3/infra/inmemory/include/infra/inmemory/in_memory_session_token_issuer.hpp`
- `src/v3/infra/inmemory/include/infra/inmemory/in_memory_message_broadcaster.hpp`
- `src/v3/infra/inmemory/include/infra/inmemory/in_memory_permission_checker.hpp`
- `src/v3/infra/inmemory/include/infra/inmemory/in_memory_config_subscriber.hpp`
- `src/v3/infra/inmemory/include/infra/inmemory/in_memory_channel_store.hpp`

## Files Modified

- `src/v3/infra/inmemory/CMakeLists.txt` — documented new headers under R268 comment block

## Implementation Notes

| Fake | Storage | Test Helpers |
|------|---------|--------------|
| `InMemoryTeamRepository` | `unordered_map<TeamId, shared_ptr<Team>>` | — |
| `InMemoryMailStore` | `unordered_map<string, vector<MailMessage>>` | — |
| `InMemoryNewsStore` | `vector<NewsItem>` (oldest-first, returned newest-first) | — |
| `InMemoryHelpfileSource` | `unordered_map<string, string>` | `set(cmd, text)` |
| `InMemoryIconProvider` | `unordered_map<string, string>` + `vector<byte>` | `set_icon()`, `set_raw_data()` |
| `InMemoryRandomSource` | `mt19937` seeded via `random_device` (or fixed seed ctor) | fixed-seed constructor |
| `InMemorySessionTokenIssuer` | atomic counter + `unordered_map<string, AccountId>` | — |
| `InMemoryMessageBroadcaster` | `vector<string>` log | `last_messages()`, `clear()` |
| `InMemoryPermissionChecker` | `unordered_map<AccountId, set<Permission>>` + groups | `grant()`, `revoke()`, `grant_group()`, `revoke_group()` |
| `InMemoryConfigSubscriber` | `vector<Callback>` + reload counter | `on_reload()`, `notify()`, `reload_count()` |
| `InMemoryChannelStore` | `unordered_map<string, ChannelDefinition>` | — |

## Skipped Ports

None — all 11 port headers existed and were implemented.
