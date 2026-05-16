# Runtime Service Library

Shared infrastructure for PvPGN services (bnetd, d2cs, d2dbs).

## Overview

The runtime library consolidates boilerplate code that was previously duplicated across service implementations:

- **Service Host** (`service_host.hpp/cpp`) — Main service lifecycle management
- **CLI Parsing** (`cli.cpp`) — Command-line argument parsing
- **Daemonization** (`daemonize.cpp`) — Unix process daemonization
- **Windows Services** (`win_service.cpp`) — Windows Service Control Manager integration
- **Crash Handler** (`crash_handler.cpp`) — Stack trace generation for debugging
- **PeerLink** (`peer_link.hpp/cpp`) — Inter-service communication with TLS + JWT

## Architecture

### Service Composition Root

Each service implements `IServiceComposition` to define its dependencies:

```cpp
class D2csComposition : public IServiceComposition {
public:
    std::string service_name() const override { return "d2cs"; }
    
    Result<void, std::string> init(const ServiceConfig& config) override {
        // Create and wire up dependencies
        db_ = std::make_unique<CharacterDatabase>(config);
        game_server_ = std::make_unique<GameServer>(db_.get());
        return Result<void, std::string>();
    }
    
    Result<void, std::string> start() override {
        return game_server_->start();
    }
    
    void stop() override { game_server_->stop(); }
    void shutdown() override { game_server_.reset(); db_.reset(); }
    std::string status() const override { return game_server_->status(); }

private:
    std::unique_ptr<CharacterDatabase> db_;
    std::unique_ptr<GameServer> game_server_;
};
```

### Service Entry Point

Services use the `run_service<>()` template helper:

```cpp
int main(int argc, char** argv) {
    return run_service<D2csComposition>(argc, argv);
}
```

This eliminates duplicated `main()`, argument parsing, signal handling, and daemonization logic.

## Configuration

Services accept command-line arguments:

```bash
./d2cs -c /etc/pvpgn/d2cs.conf -f -l debug
```

Supported options:

- `-c, --config FILE` — Configuration file path
- `-f, --foreground` — Run in foreground (don't daemonize)
- `-l, --log-level LEVEL` — Log level (trace, debug, info, warn, error, critical)
- `--log-file FILE` — Log file path (empty = stdout)
- `--pid-file FILE` — PID file path
- `-w, --work-dir DIR` — Working directory (default: .)
- `-u, --user USER` — User to run as (Unix only)
- `-g, --group GROUP` — Group to run as (Unix only)
- `-h, --help` — Show help message
- `-v, --version` — Show version information

## Inter-Service Communication (PeerLink)

Services communicate securely using PeerLink:

### Server Side (d2cs)

```cpp
PeerLinkServer server("d2cs", 6119);
server.register_handler("character.lock", [](const PeerMessage& req) {
    return handle_character_lock(req);
});
server.start();
```

### Client Side (d2dbs)

```cpp
CapabilityToken token;
token.issuer = "d2dbs";
token.subject = "d2cs";
token.capabilities = {"character.read", "character.write"};

PeerLinkClient client("d2dbs", "d2cs", "localhost:6119", token);
auto result = client.call("character.lock", request_payload);
```

## Signal Handling

The service host automatically handles:

- `SIGTERM` — Graceful shutdown
- `SIGINT` — Graceful shutdown (Ctrl+C)
- `SIGHUP` — Configuration reload (Unix only)

## Crash Handling

On crash, the service generates a stack trace:

```
=== CRASH DETECTED ===
Signal: 11 (Segmentation fault)

Stack trace:
  [0] /usr/bin/d2cs : main+0x123
  [1] /usr/bin/d2cs : GameServer::handle_connection()+0x456
  ...
=== END CRASH REPORT ===
```

## Platform Support

- **Unix/Linux** — Full support (daemonization, signals, stack traces)
- **Windows** — Service Control Manager integration, stack traces via DbgHelp
- **macOS** — Same as Unix/Linux

## Building

The runtime library is built as part of the v3 sub-tree:

```bash
cmake -DPVPGN_BUILD_V3=ON ..
make
```

## Future Enhancements

- [ ] Configuration file parsing (TOML/YAML)
- [ ] Structured logging (spdlog integration)
- [ ] Metrics collection (Prometheus)
- [ ] Health check endpoints
- [ ] Graceful shutdown with timeout
- [ ] Hot reload of plugins
- [ ] Service discovery
