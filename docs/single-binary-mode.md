# Single-Binary Mode Guide

## Overview

Single-binary mode allows all three PvPGN-PRO v3 services (bnetd, d2cs, d2dbs) to run in a single process, sharing an in-memory service registry. This is useful for:

- **Development**: Simplified local testing without managing multiple processes
- **Small deployments**: Reduced resource overhead for small-scale servers
- **Docker/containerization**: Single container image for all services
- **Testing**: Easier integration testing with all services in one process

## Architecture

In single-binary mode, the three services run in a single process with the following architecture:

```
┌─────────────────────────────────────────────────────┐
│  PvPGN Combined Process (pvpgn)                     │
├─────────────────────────────────────────────────────┤
│                                                     │
│  ┌──────────────────────────────────────────────┐  │
│  │  InMemoryServiceRegistry (shared)            │  │
│  │  - bnetd: 127.0.0.1:6112                     │  │
│  │  - d2cs:  127.0.0.1:6113                     │  │
│  │  - d2dbs: 127.0.0.1:6114                     │  │
│  └──────────────────────────────────────────────┘  │
│                                                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────┐  │
│  │   bnetd      │  │    d2cs      │  │  d2dbs   │  │
│  │  Service     │  │  Service     │  │ Service  │  │
│  │              │  │              │  │          │  │
│  │ Port: 6112   │  │ Port: 6113   │  │ Port:    │  │
│  │              │  │              │  │ 6114     │  │
│  └──────────────┘  └──────────────┘  └──────────┘  │
│                                                     │
└─────────────────────────────────────────────────────┘
```

Each service:
- Registers its endpoint in the shared registry on startup
- Can discover other services via the registry
- Communicates with other services using the registry for endpoint lookup
- Runs in its own execution context (fiber/thread)

## Building Single-Binary Mode

### Prerequisites

- CMake 3.20+
- C++20 compiler (GCC 10+, Clang 12+, MSVC 2019+)
- Standard PvPGN-PRO v3 dependencies

### Build Instructions

```bash
cd /path/to/pvpgn
mkdir build
cd build

# Enable single-binary mode
cmake -DPVPGN_SINGLE_BINARY=ON ..

# Build
cmake --build . --config Release

# The executable will be at: ./src/v3/services/combined/pvpgn
```

### Build Options

- `-DPVPGN_SINGLE_BINARY=ON`: Enable single-binary mode (default: OFF)
- `-DCMAKE_BUILD_TYPE=Release`: Build in release mode (recommended for production)
- `-DCMAKE_BUILD_TYPE=Debug`: Build in debug mode (for development)

## Running Single-Binary Mode

### Basic Usage

```bash
./pvpgn
```

Output:
```
[combined] Initializing combined single-binary mode
[combined] Service endpoints registered
[combined] Starting all services
[combined] bnetd started
[combined] d2cs started
[combined] d2dbs started
[pvpgn] All services started. Press Ctrl+C to stop.
```

### Graceful Shutdown

Press `Ctrl+C` to gracefully shut down all services:

```
^C[pvpgn] Shutting down...
[combined] Stopping all services
[pvpgn] Shutdown complete.
```

## Configuration

### Single Configuration File

In single-binary mode, you can use a single configuration file that specifies settings for all three services:

```ini
# pvpgn.conf
[bnetd]
port = 6112
max_connections = 1000

[d2cs]
port = 6113
max_connections = 500

[d2dbs]
port = 6114
max_connections = 500
```

### Per-Service Configuration

Alternatively, you can use separate configuration files for each service:

```bash
./pvpgn --config-dir ./conf/
```

The directory should contain (with `PVPGN_BUILD_V3=ON`, the only
supported build):
- `bnetd.toml`
- `d2cs.toml`
- `d2dbs.toml`

See [toml-migration.md](toml-migration.md) for a `.conf` -> `.toml`
walkthrough if you are upgrading from a legacy install.

## Service Discovery

Services discover each other using the shared `InMemoryServiceRegistry`:

### Example: d2cs discovering d2dbs

```cpp
// In d2cs service code
auto& registry = composition.registry();
auto d2dbs_endpoints = registry.lookup("d2dbs");

if (!d2dbs_endpoints.empty()) {
    auto endpoint = d2dbs_endpoints[0];
    std::cout << "Found d2dbs at " << endpoint.address() << "\n";
}
```

### Service Endpoints

Default service endpoints registered on startup:

| Service | Host      | Port | Transport |
|---------|-----------|------|-----------|
| bnetd   | 127.0.0.1 | 6112 | TCP       |
| d2cs    | 127.0.0.1 | 6113 | TCP       |
| d2dbs   | 127.0.0.1 | 6114 | TCP       |

## Limitations vs. Multi-Process Deployment

### Single-Binary Mode Limitations

1. **No process isolation**: All services share the same process memory space
   - A crash in one service crashes all services
   - Memory leaks in one service affect all services

2. **Shared resource limits**: All services share system resource limits
   - File descriptor limits apply to all services combined
   - Memory limits apply to the entire process

3. **No independent scaling**: Cannot scale individual services independently
   - All services must run on the same machine
   - Cannot use separate containers for each service

4. **Debugging complexity**: Harder to debug individual services
   - Stack traces may be interleaved
   - Profiling tools see all services as one process

### Multi-Process Deployment Advantages

1. **Process isolation**: Each service runs in its own process
   - Crash in one service doesn't affect others
   - Memory leaks are isolated

2. **Independent resource limits**: Each service has its own limits
   - Can tune file descriptors per service
   - Can set memory limits per service

3. **Horizontal scaling**: Can run multiple instances of each service
   - Use load balancers for distribution
   - Scale services independently based on demand

4. **Easier debugging**: Each service has its own stack trace
   - Profiling tools can focus on individual services
   - Logs are naturally separated

## Switching Between Modes

### From Single-Binary to Multi-Process

1. Build multi-process services:
   ```bash
   cmake -DPVPGN_SINGLE_BINARY=OFF ..
   cmake --build .
   ```

2. Run each service separately:
   ```bash
   ./bnetd --config conf/bnetd.toml &
   ./d2cs --config conf/d2cs.toml &
   ./d2dbs --config conf/d2dbs.toml &
   ```

3. Services discover each other via DNS or configuration

### From Multi-Process to Single-Binary

1. Build single-binary:
   ```bash
   cmake -DPVPGN_SINGLE_BINARY=ON ..
   cmake --build .
   ```

2. Run combined binary:
   ```bash
   ./pvpgn --config conf/pvpgn.conf
   ```

## Docker Deployment

### Single-Binary Docker Image

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential cmake git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . /src
RUN cd /src && mkdir build && cd build && \
    cmake -DPVPGN_SINGLE_BINARY=ON -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . && \
    cp src/v3/services/combined/pvpgn /usr/local/bin/

WORKDIR /app
COPY conf/ ./conf/

EXPOSE 6112 6113 6114

CMD ["pvpgn"]
```

### Docker Compose Example

```yaml
version: '3.8'

services:
  pvpgn:
    build:
      context: .
      dockerfile: Dockerfile.single-binary
    ports:
      - "6112:6112"  # bnetd
      - "6113:6113"  # d2cs
      - "6114:6114"  # d2dbs
    volumes:
      - ./conf:/app/conf
      - ./data:/app/data
    environment:
      - LOG_LEVEL=info
    restart: unless-stopped
```

### Running with Docker Compose

```bash
docker-compose up -d
docker-compose logs -f pvpgn
docker-compose down
```

## Performance Considerations

### Memory Usage

Single-binary mode uses less memory than three separate processes:

- **Single-binary**: ~150-200 MB (estimated)
- **Multi-process**: ~300-400 MB (estimated, 3 × ~100-150 MB per service)

### CPU Usage

Single-binary mode may have slightly lower CPU overhead due to:
- Fewer context switches
- Shared memory for service discovery
- No inter-process communication overhead

However, the difference is typically negligible for most workloads.

### Latency

Service-to-service communication latency is lower in single-binary mode:
- **Single-binary**: In-process function calls (~microseconds)
- **Multi-process**: TCP/IP communication (~milliseconds)

This can be significant for high-frequency inter-service communication.

## Troubleshooting

### Services Not Starting

Check the initialization logs:
```bash
./pvpgn 2>&1 | grep -i error
```

### Port Already in Use

If ports 6112-6114 are already in use:
1. Stop other services using those ports
2. Or modify the port configuration in the code

### Memory Issues

Monitor memory usage:
```bash
# Linux
ps aux | grep pvpgn
top -p $(pgrep pvpgn)

# macOS
ps aux | grep pvpgn
top -pid $(pgrep pvpgn)
```

### Debugging

Enable debug logging:
```bash
./pvpgn --log-level debug
```

## Testing

### Unit Tests

Run service discovery tests:
```bash
ctest -R ServiceRegistryTest -V
```

Run combined composition tests:
```bash
ctest -R CombinedCompositionTest -V
```

### Integration Tests

Test service discovery:
```cpp
InMemoryServiceRegistry reg;
auto ep = ServiceEndpoint{...};
reg.register_service(ep);
auto found = reg.lookup("bnetd");
assert(!found.empty());
```

## Future Enhancements

Potential improvements to single-binary mode:

1. **Dynamic service loading**: Load/unload services at runtime
2. **Service isolation**: Use sandboxing or containers within the process
3. **Advanced monitoring**: Per-service metrics and health checks
4. **Configuration hot-reload**: Update configuration without restart
5. **Service dependencies**: Automatic startup order based on dependencies

## See Also

- [Service Discovery Architecture](../refactoring-plan-05-infrastructure.md)
- [Hexagonal Architecture](../refactoring-plan-01-current-architecture.md)
- [Networking and Boost.Fiber](../refactoring-plan-06-networking-and-boost-fiber.md)
