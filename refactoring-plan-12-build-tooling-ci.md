# 12 · Build, Tooling, CI/CD

## 1. CMake modernisation

Current build: CMake 3.1 minimum, hand-edited source lists in each
`CMakeLists.txt`, `add_subdirectory` chain, no presets, no exported
targets.

Target build:

* **CMake ≥ 3.21** (CMakePresets v3, `target_sources(PRIVATE FILE_SET)`).
* `CMakePresets.json` with presets: `dev-debug`, `dev-release`,
  `ci-asan`, `ci-ubsan`, `ci-tsan`, `ci-coverage`, `rel-static`,
  `win-msvc`, `linux-clang`.
* One `CMakeLists.txt` per library; sources discovered with explicit
  lists (no globs), grouped by `target_sources`.
* All deps via `FetchContent` + `find_package` fallback. Versions
  pinned in `cmake/dependencies.cmake`.
* `vcpkg.json` manifest for users who prefer vcpkg.
* `conanfile.py` (optional) for Conan 2 users.

### 1.1 Exposed CMake options

```
PVPGN_BUILD_BNETD          ON
PVPGN_BUILD_D2CS           ON
PVPGN_BUILD_D2DBS          ON
PVPGN_BUILD_TOOLS          ON
PVPGN_BUILD_TESTS          ON
PVPGN_BUILD_WEBUI          ON         # builds Vite SPA + serves it
PVPGN_BUILD_FUZZ           OFF        # requires clang
PVPGN_SINGLE_BINARY        OFF
PVPGN_ENABLE_SCRIPTING     ON
PVPGN_STORAGE_SQLITE       ON
PVPGN_STORAGE_MYSQL        OFF
PVPGN_STORAGE_POSTGRES     OFF
PVPGN_STORAGE_ODBC         OFF
PVPGN_ENABLE_METRICS       ON
PVPGN_ENABLE_TLS           ON
PVPGN_USE_BOOST_FIBER      ON         # off → fall back to thread-per-conn
PVPGN_LTO                  ON
PVPGN_SANITIZERS           ""         # "address;undefined" etc.
```

### 1.2 Dependency manifest

| Lib | Version | Purpose |
|---|---|---|
| `boost`            | ≥ 1.84 (asio, fiber, context, beast, endian, intrusive, system, stacktrace) | I/O, fibers, HTTP, utility |
| `fmt`              | ≥ 10 (already vendored, upgrade)                 | formatting |
| `spdlog`           | ≥ 1.13                                           | logging |
| `tomlplusplus`     | ≥ 3.4                                            | config |
| `sol3` + Lua 5.4   | latest                                           | scripting |
| `Catch2`           | v3 latest                                        | tests |
| `trompeloeil`      | latest                                           | mocks |
| `rapidcheck`       | latest                                           | property tests |
| `prometheus-cpp`   | latest                                           | metrics |
| `simdutf` (opt)    | latest                                           | UTF conversion |
| `openssl` or `wolfssl` | ≥ 3.0                                        | TLS, hashing |
| `sqlite3`          | system or vendored                               | embedded DB |
| `mariadb-connector-c` / `libpqxx` / `unixODBC` | optional         | external DB backends |
| `pugixml`          | **dropped** (was vendored) — replaced by `toml++` or `nlohmann::json` |

### 1.3 Header sets

Use `target_sources(... FILE_SET HEADERS BASE_DIRS ... FILES ...)` so
that installed targets export only the intended public headers.

## 2. Repository layout add-ons

```
.clang-format
.clang-tidy
.editorconfig
.cmake-format.yaml
.pre-commit-config.yaml
.github/
  workflows/
    ci.yml
    fuzz.yml
    release.yml
    docs.yml
  ISSUE_TEMPLATE/
  PULL_REQUEST_TEMPLATE.md
docs/
  adr/                    # architecture decision records
  api/                    # generated doxygen / openapi
```

## 3. Coding standards & enforcement

* `clang-format` (LLVM-derived; 100-col, 4-sp indent).
* `clang-tidy` with `cppcoreguidelines-*`, `modernize-*`,
  `bugprone-*`, `performance-*` enabled. Custom checks:
  * Forbid `strcpy`, `sprintf`, `gets`, `scanf`.
  * Forbid `std::chrono::system_clock::now()` outside `core/clock`.
  * Forbid `std::cout`/`std::printf` outside CLI tools.
* `iwyu` advisory in CI for new code.
* `cppcheck` baseline; new warnings fail CI.
* C++ Core Guidelines compliance is a release gate.

## 4. CI/CD pipelines (GitHub Actions)

### 4.1 PR pipeline (fast)

1. Lint: clang-format check, clang-tidy on changed files, cmake-format.
2. Configure + build (gcc-13 Release, clang-17 Debug+ASan).
3. Unit + component tests.
4. Web UI build + lint + unit tests.
5. SQLite migrations smoke-test.
6. Doxygen + OpenAPI generation (verify no warnings).

### 4.2 Main pipeline (heavy)

1. Full OS/compiler/sanitizer matrix.
2. MySQL + Postgres integration tests in docker services.
3. Long-running fuzz (15 min/protocol).
4. Coverage report → Codecov.
5. Benchmark comparison vs baseline branch.
6. Build docker images for `bnetd`, `d2cs`, `d2dbs`, `webui-bundled`.

### 4.3 Release pipeline

1. Tagged commit → build static binaries for linux-x86_64, linux-arm64,
   macos-universal, windows-x86_64.
2. Generate SBOM (CycloneDX).
3. Sign artifacts with cosign.
4. Publish to GitHub Releases + Docker Hub + GHCR.
5. Generate changelog from conventional commits.

## 5. Packaging

* **Debian/Ubuntu**: native `.deb` via `cmake -DCPACK_GENERATOR=DEB`,
  ships `systemd` units (`bnetd.service`, `d2cs.service`,
  `d2dbs.service`), users `pvpgn:pvpgn`, conf in `/etc/pvpgn/`.
* **RPM**: matching `.rpm` for Fedora/RHEL.
* **macOS**: Homebrew formula in a tap repo.
* **Windows**: NSIS installer + Win32 service registration.
* **Docker**: multi-stage Dockerfile, distroless final image.
* **Helm chart** under `packaging/helm/` for Kubernetes deployments.

## 6. Reproducible builds

* All deps pinned by version+hash in `cmake/dependencies.cmake`.
* `SOURCE_DATE_EPOCH` honoured.
* `--build-id=sha1`, `-fdebug-prefix-map`.
* CI runs `diffoscope` on consecutive builds for the linux-x86_64
  release artifact.

## 7. Documentation pipeline

* Doxygen → `docs/api/cpp/` from public headers only.
* OpenAPI/Redoc → `docs/api/rest/` from `openapi.yaml`.
* mdBook for narrative docs (this refactor plan, ADRs, ops guide).
* Hosted on GitHub Pages on every `main` commit.

## 8. Pre-commit hooks (`pre-commit`)

* `clang-format`, `cmake-format`, `prettier` (web), `eslint`,
  `markdownlint`, `shellcheck`, end-of-file fixer, trailing-whitespace.
* CI re-runs the same hook list to enforce consistency.
