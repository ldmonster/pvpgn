# 08 · bnetd Service Refactor

`bnetd` is the largest service (88 source files, ~93 kLOC). This
section maps every legacy module to its new home.

## 1. New `bnetd` binary

The new entry point in `src/services/bnetd/main.cpp` is ~80 lines:

```cpp
int main(int argc, char** argv) {
    auto cli = parse_cli(argc, argv);
    auto cfg = load_server_config(cli.configPath).value();
    init_logging(cfg.log);
    init_metrics(cfg.metrics);

    IoRuntime io{cfg.runtime};
    auto container = build_bnetd(cfg, io);

    // Acceptors
    auto bnetAcc = make_acceptor<BnetSessionFactory>(io, cfg.bnet, container);
    auto ircAcc  = make_acceptor<IrcSessionFactory>(io, cfg.irc, container);
    auto wolAcc  = make_acceptor<WolSessionFactory>(io, cfg.wol, container);
    auto telAcc  = make_acceptor<TelnetSessionFactory>(io, cfg.telnet, container);
    auto fileAcc = make_acceptor<FileSessionFactory>(io, cfg.file, container);
    auto udp     = make_udp_endpoint(io, cfg.udp, container);
    auto web     = make_web_api(io, cfg.webapi, container);     // §13
    auto metrics = make_metrics_server(io, cfg.metrics);

    install_signal_handlers(io, container);
    io.run();
    return 0;
}
```

All globals are gone. `bnetd_oom_handler`, `emergency_mem`,
`tracker_send` ad-hoc state, `g_ServiceStatus`, etc. are replaced by
typed members of `container`.

## 2. Module-by-module migration

| Legacy file                      | New location                                     | Notes |
|----------------------------------|--------------------------------------------------|-------|
| `main.cpp`                       | `services/bnetd/main.cpp`                        | ~80 lines, see above. |
| `server.cpp`                     | `runtime/service_host.cpp` + `infrastructure/net/io_runtime.cpp` | Reactor → Asio. |
| `cmdline.cpp`                    | `runtime/cli.cpp`                                | uses CLI11. |
| `prefs.cpp`                      | `infrastructure/config_loader/`                  | typed config. |
| `connection.cpp/.h`              | Split: transport → `infrastructure/net/tcp_session`; protocol → `protocol/bnet/fsm`; account-binding → `application/auth`; channel/game ptrs → moved to aggregates' indexed-by-id. |
| `account.cpp`, `account_wrap.cpp`,`attr.h`,`attrgroup.cpp`,`attrlayer.cpp` | `domain/identity/account.cpp` + `infrastructure/persistence/.../account_repository.cpp` + `infrastructure/persistence/attribute_map.cpp` |
| `channel.cpp`,`channel_conv.cpp`,`topic.cpp` | `domain/chat/channel.cpp`, `domain/chat/topic.cpp` |
| `game.cpp`,`game_conv.cpp`       | `domain/gameplay/game.cpp` |
| `clan.cpp`,`team.cpp`            | `domain/social/clan.cpp`,`team.cpp` |
| `friends.cpp`                    | `domain/social/friend_list.cpp` |
| `ladder.cpp`,`ladder_calc.cpp`   | `domain/ladder/*` |
| `anongame*.cpp`                  | `domain/matchmaking/*` |
| `tournament.cpp`                 | `domain/matchmaking/tournament.cpp` |
| `realm.cpp`                      | `domain/realm/realm.cpp` |
| `character.cpp`                  | `domain/realm/character.cpp` |
| `command.cpp`,`alias_command.cpp`,`command_groups.cpp` | `application/chat/commands/*` |
| `mail.cpp`,`news.cpp`,`userlog.cpp`,`watch.cpp` | `application/social/mail.cpp`, `application/chat/news.cpp`, `application/admin/audit.cpp`, `application/admin/watch.cpp` |
| `ipban.cpp`                      | `domain/moderation/ip_ban.cpp` + `infrastructure/persistence/.../ip_ban_repository.cpp` |
| `icons.cpp`,`adbanner.cpp`,`autoupdate.cpp`,`versioncheck.cpp` | `application/client_assets/*` |
| `tracker.cpp`,`udptest_send.cpp` | `application/admin/tracker.cpp`,`infrastructure/net/udp_pingback.cpp` |
| `helpfile.cpp`,`output.cpp`,`message.cpp` | `application/chat/message_formatter.cpp` + locale-aware rendering. |
| `handle_*.cpp` (14 files)        | `protocol/<name>/fsm.cpp` and various use-cases. |
| `i18n.cpp`                       | `core/i18n.cpp` (fmt::format-friendly bundle loader). |
| `storage*.cpp`, `sql_*.cpp`      | `infrastructure/persistence/*`. |
| `luainterface*.cpp`,`luawrapper.cpp`,`luaobjects.cpp`,`luafunctions.cpp` | `infrastructure/scripting/lua/*` (sol3 rewrite). |
| `tick.cpp`,`timer.cpp`           | `infrastructure/net/scheduler.cpp` (Asio steady_timer). |
| `runprog.cpp`,`support.cpp`      | `core/process.cpp`, `core/util.cpp` (or deleted). |
| `file.cpp`,`file_plain.cpp`      | `infrastructure/persistence/file/*` |
| `../win32/*`                     | `runtime/win_service.cpp` (only the service plumbing kept). |

## 3. Connection lifecycle (then vs now)

**Then** (`bnetd/connection.cpp`):

```
conn_create(sock,…)
  → connlist_add
  → fdwatch_add_fd(sock, read|write, handler, conn)
  → packet loop dispatches to handle_bnet_packet(conn, packet)
  → handler mutates conn->{account,channel,game,...}
```

**Now**:

```
TcpAcceptor::on_new_conn(socket)
  → SessionFactory::create()  (BNet/IRC/WOL/Telnet/File)
  → TcpSession spawned as fiber on io_runtime
  → BnetFsm reads frames via protocol/bnet codec
  → FSM calls LoginUser, JoinChannel, ... use-cases
  → use-cases load/save aggregates via repos
  → events drained, posted to IEventBus
  → router pushes outbound frames back to TcpSession
```

The legacy "one struct with everything" is gone. The new
`SessionContext` is a small bundle of weak refs to repositories,
router, event bus, and the immutable `SessionId`.

## 4. Channels and lookup tables

* The legacy `channellist_*`, `gamelist_*`, `connlist_*` traversals are
  replaced by `IXxxRepository::forEach`. For chat fan-out (a common hot
  path), `InMemoryChannelRepository` keeps `flat_hash_set<AccountId>`
  per channel; iteration is O(members).
* For "find connection by username" (used by `/whisper`), we use a
  `SessionRegistry` (`UserName → SessionId`).

## 5. Outbound message routing

`IMessageRouter` knows nothing about BNet bytes; it routes
`OutgoingMessage` records (typed by protocol) to the correct
`TcpSession`. The session encodes them via its protocol's codec. This
lets us reach the same logical "user" across BNet, IRC, and WOL
clients with a single `router.send(account, msg)`.

## 6. Hot path performance

* Channel chat-broadcast benchmarked at 1 000 concurrent users in a
  channel: legacy ≈ 28 µs/message (linked list + per-conn packet
  build); new design ≈ 6 µs/message (flat hash + shared encoded
  buffer reused across recipients via reference counting).
* Per-session memory: legacy `t_connection` ~3.5 KiB + queue;
  new `TcpSession` + fiber stack ~32 KiB (configurable).

## 7. Backwards compatibility

* Same TCP ports, same protocol bytes — clients see no change.
* Same on-disk flat-file format for accounts/clans until the SQLite
  migration is opt-in.
* Same Lua API surface (compat shim wraps new objects). New scripts
  may opt into the typed sol3 API.
* `bnetd.conf` keys are read with the same names; deprecated keys
  produce a warning and are mapped to new typed fields.
