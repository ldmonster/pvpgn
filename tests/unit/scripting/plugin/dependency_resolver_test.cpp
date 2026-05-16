// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include "scripting/plugin/dependency_resolver.hpp"
#include "scripting/plugin/plugin_manifest.hpp"

namespace pvpgn::scripting::plugin::test {

PluginManifest make_manifest(std::string id, std::string version,
                              std::vector<PluginDependency> deps = {}) {
    PluginManifest m;
    m.id = std::move(id);
    m.name = m.id;
    m.version = SemVer::parse(version).value();
    m.entry_point = "main.lua";
    m.dependencies = std::move(deps);
    return m;
}

TEST(DependencyResolverTest, SinglePluginNoDepsSortsCorrectly) {
    DependencyResolver resolver;
    resolver.add_plugin(make_manifest("plugin.a", "1.0.0"));
    auto result = resolver.resolve();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 1u);
    EXPECT_EQ((*result)[0].plugin_id, "plugin.a");
}

TEST(DependencyResolverTest, LinearDependencyChain) {
    DependencyResolver resolver;
    resolver.add_plugin(make_manifest("plugin.c", "1.0.0", {{"plugin.b", ">=1.0.0"}}));
    resolver.add_plugin(make_manifest("plugin.b", "1.0.0", {{"plugin.a", ">=1.0.0"}}));
    resolver.add_plugin(make_manifest("plugin.a", "1.0.0"));

    auto result = resolver.resolve();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 3u);
    // a must come before b, b before c
    auto ids = std::vector<std::string>{};
    for (auto& r : *result) ids.push_back(r.plugin_id);
    auto pos_a = std::find(ids.begin(), ids.end(), "plugin.a") - ids.begin();
    auto pos_b = std::find(ids.begin(), ids.end(), "plugin.b") - ids.begin();
    auto pos_c = std::find(ids.begin(), ids.end(), "plugin.c") - ids.begin();
    EXPECT_LT(pos_a, pos_b);
    EXPECT_LT(pos_b, pos_c);
}

TEST(DependencyResolverTest, MultipleDependencies) {
    DependencyResolver resolver;
    resolver.add_plugin(make_manifest("plugin.d", "1.0.0", 
        {{"plugin.b", ">=1.0.0"}, {"plugin.c", ">=1.0.0"}}));
    resolver.add_plugin(make_manifest("plugin.b", "1.0.0", {{"plugin.a", ">=1.0.0"}}));
    resolver.add_plugin(make_manifest("plugin.c", "1.0.0", {{"plugin.a", ">=1.0.0"}}));
    resolver.add_plugin(make_manifest("plugin.a", "1.0.0"));

    auto result = resolver.resolve();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 4u);
    
    auto ids = std::vector<std::string>{};
    for (auto& r : *result) ids.push_back(r.plugin_id);
    auto pos_a = std::find(ids.begin(), ids.end(), "plugin.a") - ids.begin();
    auto pos_b = std::find(ids.begin(), ids.end(), "plugin.b") - ids.begin();
    auto pos_c = std::find(ids.begin(), ids.end(), "plugin.c") - ids.begin();
    auto pos_d = std::find(ids.begin(), ids.end(), "plugin.d") - ids.begin();
    
    EXPECT_LT(pos_a, pos_b);
    EXPECT_LT(pos_a, pos_c);
    EXPECT_LT(pos_b, pos_d);
    EXPECT_LT(pos_c, pos_d);
}

TEST(DependencyResolverTest, MissingRequiredDepReturnsError) {
    DependencyResolver resolver;
    resolver.add_plugin(make_manifest("plugin.b", "1.0.0", {{"plugin.a", ">=1.0.0"}}));
    // plugin.a not registered
    auto result = resolver.resolve();
    EXPECT_FALSE(result.has_value());
}

TEST(DependencyResolverTest, VersionMismatchReturnsError) {
    DependencyResolver resolver;
    resolver.add_plugin(make_manifest("plugin.a", "1.0.0")); // provides 1.0.0
    resolver.add_plugin(make_manifest("plugin.b", "1.0.0", {{"plugin.a", ">=2.0.0"}})); // needs 2.x
    auto result = resolver.resolve();
    EXPECT_FALSE(result.has_value());
}

TEST(DependencyResolverTest, CircularDependencyReturnsError) {
    DependencyResolver resolver;
    resolver.add_plugin(make_manifest("plugin.a", "1.0.0", {{"plugin.b", ">=1.0.0"}}));
    resolver.add_plugin(make_manifest("plugin.b", "1.0.0", {{"plugin.a", ">=1.0.0"}}));
    auto result = resolver.resolve();
    EXPECT_FALSE(result.has_value());
}

TEST(DependencyResolverTest, OptionalMissingDepDoesNotFail) {
    DependencyResolver resolver;
    PluginDependency opt_dep{"plugin.optional", ">=1.0.0", true};
    resolver.add_plugin(make_manifest("plugin.a", "1.0.0", {opt_dep}));
    auto result = resolver.resolve();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 1u);
}

TEST(DependencyResolverTest, ConflictingPluginsReturnError) {
    DependencyResolver resolver;
    PluginManifest a = make_manifest("plugin.a", "1.0.0");
    a.conflicts = {"plugin.b"};
    resolver.add_plugin(std::move(a));
    resolver.add_plugin(make_manifest("plugin.b", "1.0.0"));
    auto result = resolver.resolve();
    EXPECT_FALSE(result.has_value());
}

TEST(DependencyResolverTest, RegisteredPluginsReturnsAll) {
    DependencyResolver resolver;
    resolver.add_plugin(make_manifest("plugin.a", "1.0.0"));
    resolver.add_plugin(make_manifest("plugin.b", "1.0.0"));
    resolver.add_plugin(make_manifest("plugin.c", "1.0.0"));
    
    auto plugins = resolver.registered_plugins();
    EXPECT_EQ(plugins.size(), 3u);
    EXPECT_NE(std::find(plugins.begin(), plugins.end(), "plugin.a"), plugins.end());
    EXPECT_NE(std::find(plugins.begin(), plugins.end(), "plugin.b"), plugins.end());
    EXPECT_NE(std::find(plugins.begin(), plugins.end(), "plugin.c"), plugins.end());
}

TEST(DependencyResolverTest, CheckPluginSuccess) {
    DependencyResolver resolver;
    resolver.add_plugin(make_manifest("plugin.a", "1.0.0"));
    resolver.add_plugin(make_manifest("plugin.b", "1.0.0", {{"plugin.a", ">=1.0.0"}}));
    
    auto check = resolver.check_plugin("plugin.b");
    EXPECT_TRUE(check.has_value());
}

TEST(DependencyResolverTest, CheckPluginNotFound) {
    DependencyResolver resolver;
    auto check = resolver.check_plugin("nonexistent");
    EXPECT_FALSE(check.has_value());
}

TEST(DependencyResolverTest, ResolvedPluginHasDependencyInfo) {
    DependencyResolver resolver;
    resolver.add_plugin(make_manifest("plugin.a", "1.0.0"));
    resolver.add_plugin(make_manifest("plugin.b", "1.0.0", {{"plugin.a", ">=1.0.0"}}));
    
    auto result = resolver.resolve();
    ASSERT_TRUE(result.has_value());
    
    // Find plugin.b in results
    auto it = std::find_if(result->begin(), result->end(),
        [](const ResolvedPlugin& p) { return p.plugin_id == "plugin.b"; });
    ASSERT_NE(it, result->end());
    
    // Check that it has plugin.a as a dependency
    EXPECT_NE(std::find(it->load_order_deps.begin(), it->load_order_deps.end(), "plugin.a"),
              it->load_order_deps.end());
}

} // namespace pvpgn::scripting::plugin::test
