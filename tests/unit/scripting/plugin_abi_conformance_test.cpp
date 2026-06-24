// SPDX-License-Identifier: GPL-2.0-or-later
// Plugin C ABI Conformance Tests
//
// These tests verify the public plugin infrastructure:
//   • PluginManifest parsing from plugin.toml content
//   • SemVer comparison semantics
//   • DependencyResolver load-order and error paths
//   • Plugin ID format validation  ([a-z][a-z0-9.-]*)
//   • api_version_req compatibility check via SemVer::satisfies()
//
// All tests use Catch2 v3 syntax (TEST_CASE / SECTION / REQUIRE).

#include <catch2/catch_test_macros.hpp>

#include "scripting/plugin/plugin_manifest.hpp"
#include "scripting/plugin/semver.hpp"
#include "scripting/plugin/dependency_resolver.hpp"

#include <algorithm>
#include <regex>
#include <string>
#include <vector>

using namespace pvpgn::scripting::plugin;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

/// Minimal valid plugin.toml content (mirrors plugins/example-quiz/plugin.toml)
static constexpr std::string_view kExampleQuizToml = R"toml(
[plugin]
id = "com.pvpgn.example.quiz"
name = "Quiz Plugin"
version = "1.0.0"
description = "Example quiz game plugin for PvPGN"
author = "PvPGN Team"
license = "GPL-2.0"
entry_point = "main.lua"
api_version_req = ">=3.0.0"
provides = ["game.quiz"]
conflicts = []

[[dependencies]]
plugin_id = "com.pvpgn.core.chat"
version_req = ">=3.0.0"
optional = false
)toml";

/// Build a PluginManifest programmatically (no TOML parsing needed)
static PluginManifest make_manifest(std::string id,
                                    std::string version_str,
                                    std::vector<PluginDependency> deps = {})
{
    PluginManifest m;
    m.id          = std::move(id);
    m.name        = m.id;
    m.version     = SemVer::parse(version_str).value();
    m.entry_point = "main.lua";
    m.dependencies = std::move(deps);
    return m;
}

/// Regex that every valid plugin ID must match: [a-z][a-z0-9.-]*
static const std::regex kPluginIdPattern{R"([a-z][a-z0-9.\-]*)"};

static bool is_valid_plugin_id(std::string_view id) {
    return std::regex_match(std::string(id), kPluginIdPattern);
}

// ─────────────────────────────────────────────────────────────────────────────
// PluginManifest parsing
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("PluginManifest: parse example-quiz plugin.toml", "[plugin][manifest]") {
    auto result = PluginManifest::parse_toml(kExampleQuizToml);

    REQUIRE(result.has_value());
    const auto& m = result.value();

    SECTION("id is parsed correctly") {
        REQUIRE(m.id == "com.pvpgn.example.quiz");
    }

    SECTION("name is parsed correctly") {
        REQUIRE(m.name == "Quiz Plugin");
    }

    SECTION("version is parsed correctly") {
        REQUIRE(m.version.major == 1u);
        REQUIRE(m.version.minor == 0u);
        REQUIRE(m.version.patch == 0u);
    }

    SECTION("entry_point is parsed correctly") {
        REQUIRE(m.entry_point == "main.lua");
    }

    SECTION("api_version_req is parsed correctly") {
        REQUIRE(m.api_version_req == ">=3.0.0");
    }

    SECTION("provides list is parsed correctly") {
        REQUIRE(m.provides.size() == 1u);
        REQUIRE(m.provides[0] == "game.quiz");
    }

    SECTION("conflicts list is empty") {
        REQUIRE(m.conflicts.empty());
    }

    SECTION("dependency is parsed correctly") {
        REQUIRE(m.dependencies.size() == 1u);
        REQUIRE(m.dependencies[0].plugin_id == "com.pvpgn.core.chat");
        REQUIRE(m.dependencies[0].version_req == ">=3.0.0");
        REQUIRE(m.dependencies[0].optional == false);
    }
}

TEST_CASE("PluginManifest: parse minimal manifest (no optional fields)", "[plugin][manifest]") {
    constexpr std::string_view kMinimal = R"toml(
[plugin]
id = "com.example.minimal"
name = "Minimal"
version = "0.1.0"
entry_point = "main.lua"
)toml";

    auto result = PluginManifest::parse_toml(kMinimal);
    REQUIRE(result.has_value());
    const auto& m = result.value();
    REQUIRE(m.id == "com.example.minimal");
    REQUIRE(m.version.major == 0u);
    REQUIRE(m.version.minor == 1u);
    REQUIRE(m.dependencies.empty());
    REQUIRE(m.provides.empty());
}

TEST_CASE("PluginManifest: parse manifest with optional dependency", "[plugin][manifest]") {
    constexpr std::string_view kWithOptDep = R"toml(
[plugin]
id = "com.example.withopt"
name = "WithOpt"
version = "1.0.0"
entry_point = "main.lua"

[[dependencies]]
plugin_id = "com.example.optional-dep"
version_req = ">=1.0.0"
optional = true
)toml";

    auto result = PluginManifest::parse_toml(kWithOptDep);
    REQUIRE(result.has_value());
    const auto& m = result.value();
    REQUIRE(m.dependencies.size() == 1u);
    REQUIRE(m.dependencies[0].optional == true);
}

TEST_CASE("PluginManifest: invalid version string returns error", "[plugin][manifest]") {
    constexpr std::string_view kBadVersion = R"toml(
[plugin]
id = "com.example.bad"
name = "Bad"
version = "not-a-semver"
entry_point = "main.lua"
)toml";

    auto result = PluginManifest::parse_toml(kBadVersion);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("PluginManifest: to_toml round-trip preserves key fields", "[plugin][manifest]") {
    auto parsed = PluginManifest::parse_toml(kExampleQuizToml);
    REQUIRE(parsed.has_value());

    std::string serialized = parsed.value().to_toml();
    REQUIRE_FALSE(serialized.empty());

    // Re-parse the serialized form
    auto reparsed = PluginManifest::parse_toml(serialized);
    REQUIRE(reparsed.has_value());
    REQUIRE(reparsed.value().id == parsed.value().id);
    REQUIRE(reparsed.value().version == parsed.value().version);
    REQUIRE(reparsed.value().entry_point == parsed.value().entry_point);
}

// ─────────────────────────────────────────────────────────────────────────────
// SemVer comparison
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("SemVer: basic parse and comparison", "[plugin][semver]") {
    auto r100 = SemVer::parse("1.0.0");
    auto r110 = SemVer::parse("1.1.0");
    auto r200 = SemVer::parse("2.0.0");

    REQUIRE(r100.has_value());
    REQUIRE(r110.has_value());
    REQUIRE(r200.has_value());

    const auto v100 = r100.value();
    const auto v110 = r110.value();
    const auto v200 = r200.value();

    SECTION("ordering") {
        REQUIRE(v100 < v110);
        REQUIRE(v110 < v200);
        REQUIRE(v200 > v100);
        REQUIRE(v100 == SemVer::parse("1.0.0").value());
    }

    SECTION("prerelease is lower than release") {
        auto pre = SemVer::parse("1.0.0-alpha").value();
        REQUIRE(pre < v100);
        REQUIRE(v100 > pre);
    }

    SECTION("build metadata is ignored in comparison") {
        auto b1 = SemVer::parse("1.0.0+build.1").value();
        auto b2 = SemVer::parse("1.0.0+build.2").value();
        REQUIRE(b1 == b2);
    }
}

TEST_CASE("SemVer: satisfies() — >= operator", "[plugin][semver]") {
    auto v300 = SemVer::parse("3.0.0").value();
    auto v310 = SemVer::parse("3.1.0").value();
    auto v299 = SemVer::parse("2.9.9").value();

    REQUIRE(v300.satisfies(">=3.0.0"));
    REQUIRE(v310.satisfies(">=3.0.0"));
    REQUIRE_FALSE(v299.satisfies(">=3.0.0"));
}

TEST_CASE("SemVer: satisfies() — caret (^) compatible range", "[plugin][semver]") {
    auto v120 = SemVer::parse("1.2.0").value();
    auto v130 = SemVer::parse("1.3.0").value();
    auto v200 = SemVer::parse("2.0.0").value();

    REQUIRE(v120.satisfies("^1.2.0"));
    REQUIRE(v130.satisfies("^1.2.0"));
    REQUIRE_FALSE(v200.satisfies("^1.2.0"));
}

TEST_CASE("SemVer: satisfies() — tilde (~) patch range", "[plugin][semver]") {
    auto v123 = SemVer::parse("1.2.3").value();
    auto v124 = SemVer::parse("1.2.4").value();
    auto v130 = SemVer::parse("1.3.0").value();

    REQUIRE(v123.satisfies("~1.2.3"));
    REQUIRE(v124.satisfies("~1.2.3"));
    REQUIRE_FALSE(v130.satisfies("~1.2.3"));
}

TEST_CASE("SemVer: satisfies() — exact match (=)", "[plugin][semver]") {
    auto v100 = SemVer::parse("1.0.0").value();
    auto v101 = SemVer::parse("1.0.1").value();

    REQUIRE(v100.satisfies("=1.0.0"));
    REQUIRE_FALSE(v101.satisfies("=1.0.0"));
}

TEST_CASE("SemVer: satisfies() — not-equal (!=)", "[plugin][semver]") {
    auto v100 = SemVer::parse("1.0.0").value();
    auto v101 = SemVer::parse("1.0.1").value();

    REQUIRE_FALSE(v100.satisfies("!=1.0.0"));
    REQUIRE(v101.satisfies("!=1.0.0"));
}

TEST_CASE("SemVer: to_string round-trip", "[plugin][semver]") {
    const std::string ver = "2.3.4-rc.1+sha.abc";
    auto r = SemVer::parse(ver);
    REQUIRE(r.has_value());
    REQUIRE(r.value().to_string() == ver);
}

TEST_CASE("SemVer: parse rejects invalid strings", "[plugin][semver]") {
    REQUIRE_FALSE(SemVer::parse("").has_value());
    REQUIRE_FALSE(SemVer::parse("1.2").has_value());
    REQUIRE_FALSE(SemVer::parse("a.b.c").has_value());
    REQUIRE_FALSE(SemVer::parse("1.2.3.4").has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
// DependencyResolver
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("DependencyResolver: single plugin with no deps", "[plugin][resolver]") {
    DependencyResolver resolver;
    resolver.add_plugin(make_manifest("plugin.a", "1.0.0"));

    auto result = resolver.resolve();
    REQUIRE(result.has_value());
    REQUIRE(result.value().size() == 1u);
    REQUIRE(result.value()[0].plugin_id == "plugin.a");
}

TEST_CASE("DependencyResolver: linear dependency chain produces correct order", "[plugin][resolver]") {
    DependencyResolver resolver;
    resolver.add_plugin(make_manifest("plugin.c", "1.0.0", {{"plugin.b", ">=1.0.0"}}));
    resolver.add_plugin(make_manifest("plugin.b", "1.0.0", {{"plugin.a", ">=1.0.0"}}));
    resolver.add_plugin(make_manifest("plugin.a", "1.0.0"));

    auto result = resolver.resolve();
    REQUIRE(result.has_value());
    const auto& resolved = result.value();
    REQUIRE(resolved.size() == 3u);

    std::vector<std::string> ids;
    for (const auto& r : resolved) ids.push_back(r.plugin_id);

    auto pos = [&](const std::string& id) {
        return static_cast<int>(std::find(ids.begin(), ids.end(), id) - ids.begin());
    };
    REQUIRE(pos("plugin.a") < pos("plugin.b"));
    REQUIRE(pos("plugin.b") < pos("plugin.c"));
}

TEST_CASE("DependencyResolver: missing required dependency returns error", "[plugin][resolver]") {
    DependencyResolver resolver;
    resolver.add_plugin(make_manifest("plugin.b", "1.0.0", {{"plugin.a", ">=1.0.0"}}));
    // plugin.a not registered

    auto result = resolver.resolve();
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("DependencyResolver: version mismatch returns error", "[plugin][resolver]") {
    DependencyResolver resolver;
    resolver.add_plugin(make_manifest("plugin.a", "1.0.0"));
    resolver.add_plugin(make_manifest("plugin.b", "1.0.0", {{"plugin.a", ">=2.0.0"}}));

    auto result = resolver.resolve();
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("DependencyResolver: circular dependency returns error", "[plugin][resolver]") {
    DependencyResolver resolver;
    resolver.add_plugin(make_manifest("plugin.a", "1.0.0", {{"plugin.b", ">=1.0.0"}}));
    resolver.add_plugin(make_manifest("plugin.b", "1.0.0", {{"plugin.a", ">=1.0.0"}}));

    auto result = resolver.resolve();
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("DependencyResolver: optional missing dependency does not fail", "[plugin][resolver]") {
    DependencyResolver resolver;
    PluginDependency opt_dep{"plugin.optional", ">=1.0.0", true};
    resolver.add_plugin(make_manifest("plugin.a", "1.0.0", {opt_dep}));

    auto result = resolver.resolve();
    REQUIRE(result.has_value());
    REQUIRE(result.value().size() == 1u);
}

TEST_CASE("DependencyResolver: conflict detection returns error", "[plugin][resolver]") {
    DependencyResolver resolver;

    PluginManifest a = make_manifest("plugin.a", "1.0.0");
    a.conflicts.push_back("plugin.b");

    PluginManifest b = make_manifest("plugin.b", "1.0.0");

    resolver.add_plugin(std::move(a));
    resolver.add_plugin(std::move(b));

    auto result = resolver.resolve();
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("DependencyResolver: registered_plugins returns all IDs", "[plugin][resolver]") {
    DependencyResolver resolver;
    resolver.add_plugin(make_manifest("plugin.a", "1.0.0"));
    resolver.add_plugin(make_manifest("plugin.b", "1.0.0"));

    auto ids = resolver.registered_plugins();
    REQUIRE(ids.size() == 2u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Plugin ID format validation
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Plugin ID format: valid IDs match [a-z][a-z0-9.-]*", "[plugin][id-format]") {
    SECTION("simple lowercase") {
        REQUIRE(is_valid_plugin_id("myplugin"));
    }
    SECTION("reverse-domain notation") {
        REQUIRE(is_valid_plugin_id("com.pvpgn.example.quiz"));
    }
    SECTION("with digits") {
        REQUIRE(is_valid_plugin_id("plugin2"));
    }
    SECTION("with hyphens") {
        REQUIRE(is_valid_plugin_id("my-plugin"));
    }
    SECTION("example-quiz id from plugin.toml") {
        REQUIRE(is_valid_plugin_id("com.pvpgn.example.quiz"));
    }
}

TEST_CASE("Plugin ID format: invalid IDs are rejected", "[plugin][id-format]") {
    SECTION("starts with digit") {
        REQUIRE_FALSE(is_valid_plugin_id("1plugin"));
    }
    SECTION("starts with uppercase") {
        REQUIRE_FALSE(is_valid_plugin_id("MyPlugin"));
    }
    SECTION("contains uppercase") {
        REQUIRE_FALSE(is_valid_plugin_id("my_Plugin"));
    }
    SECTION("empty string") {
        REQUIRE_FALSE(is_valid_plugin_id(""));
    }
    SECTION("starts with dot") {
        REQUIRE_FALSE(is_valid_plugin_id(".plugin"));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// API version compatibility check
// ─────────────────────────────────────────────────────────────────────────────

/// Simulates the server's current pvpgn API version
static const SemVer kServerApiVersion = SemVer::parse("3.0.0").value();

/// Returns true if the plugin's api_version_req is satisfied by the server
static bool is_api_compatible(const PluginManifest& manifest) {
    if (manifest.api_version_req.empty()) {
        return true; // no requirement → always compatible
    }
    return kServerApiVersion.satisfies(manifest.api_version_req);
}

TEST_CASE("API version compatibility: plugin requiring >=3.0.0 is compatible with server 3.0.0",
          "[plugin][api-compat]")
{
    auto manifest = PluginManifest::parse_toml(kExampleQuizToml);
    REQUIRE(manifest.has_value());
    REQUIRE(is_api_compatible(manifest.value()));
}

TEST_CASE("API version compatibility: plugin requiring >=4.0.0 is incompatible with server 3.0.0",
          "[plugin][api-compat]")
{
    constexpr std::string_view kFuturePlugin = R"toml(
[plugin]
id = "com.example.future"
name = "Future"
version = "1.0.0"
entry_point = "main.lua"
api_version_req = ">=4.0.0"
)toml";

    auto manifest = PluginManifest::parse_toml(kFuturePlugin);
    REQUIRE(manifest.has_value());
    REQUIRE_FALSE(is_api_compatible(manifest.value()));
}

TEST_CASE("API version compatibility: plugin with no api_version_req is always compatible",
          "[plugin][api-compat]")
{
    constexpr std::string_view kNoReq = R"toml(
[plugin]
id = "com.example.noreq"
name = "NoReq"
version = "1.0.0"
entry_point = "main.lua"
)toml";

    auto manifest = PluginManifest::parse_toml(kNoReq);
    REQUIRE(manifest.has_value());
    REQUIRE(is_api_compatible(manifest.value()));
}

TEST_CASE("API version compatibility: caret range ^3.0.0 is compatible with server 3.0.0",
          "[plugin][api-compat]")
{
    constexpr std::string_view kCaret = R"toml(
[plugin]
id = "com.example.caret"
name = "Caret"
version = "1.0.0"
entry_point = "main.lua"
api_version_req = "^3.0.0"
)toml";

    auto manifest = PluginManifest::parse_toml(kCaret);
    REQUIRE(manifest.has_value());
    REQUIRE(is_api_compatible(manifest.value()));
}
