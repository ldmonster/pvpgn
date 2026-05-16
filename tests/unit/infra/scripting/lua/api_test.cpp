#include <gtest/gtest.h>
#include "infra/scripting/lua/plugin_store.hpp"
#include "infra/scripting/lua/lua_event_bus.hpp"
#include "infra/scripting/lua/lua_command_registry.hpp"
#include "infra/scripting/lua/simple_http_client.hpp"

namespace pvpgn::infra::scripting::test {

// ============================================================================
// PluginStore Tests
// ============================================================================

class PluginStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any existing data
        store_.clear("test_plugin");
    }
    
    PluginStore& store_ = PluginStore::instance();
};

TEST_F(PluginStoreTest, SetAndGet) {
    store_.set("test_plugin", "key1", "value1");
    auto result = store_.get("test_plugin", "key1");
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "value1");
}

TEST_F(PluginStoreTest, GetNonexistent) {
    auto result = store_.get("test_plugin", "nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(PluginStoreTest, SetMultipleKeys) {
    store_.set("test_plugin", "key1", "value1");
    store_.set("test_plugin", "key2", "value2");
    store_.set("test_plugin", "key3", "value3");
    
    EXPECT_EQ(*store_.get("test_plugin", "key1"), "value1");
    EXPECT_EQ(*store_.get("test_plugin", "key2"), "value2");
    EXPECT_EQ(*store_.get("test_plugin", "key3"), "value3");
}

TEST_F(PluginStoreTest, Delete) {
    store_.set("test_plugin", "key1", "value1");
    EXPECT_TRUE(store_.remove("test_plugin", "key1"));
    EXPECT_FALSE(store_.get("test_plugin", "key1").has_value());
}

TEST_F(PluginStoreTest, DeleteNonexistent) {
    EXPECT_FALSE(store_.remove("test_plugin", "nonexistent"));
}

TEST_F(PluginStoreTest, Keys) {
    store_.set("test_plugin", "key1", "value1");
    store_.set("test_plugin", "key2", "value2");
    store_.set("test_plugin", "key3", "value3");
    
    auto keys = store_.keys("test_plugin");
    EXPECT_EQ(keys.size(), 3);
    
    std::sort(keys.begin(), keys.end());
    EXPECT_EQ(keys[0], "key1");
    EXPECT_EQ(keys[1], "key2");
    EXPECT_EQ(keys[2], "key3");
}

TEST_F(PluginStoreTest, KeysEmpty) {
    auto keys = store_.keys("test_plugin");
    EXPECT_EQ(keys.size(), 0);
}

TEST_F(PluginStoreTest, Clear) {
    store_.set("test_plugin", "key1", "value1");
    store_.set("test_plugin", "key2", "value2");
    
    store_.clear("test_plugin");
    
    auto keys = store_.keys("test_plugin");
    EXPECT_EQ(keys.size(), 0);
}

TEST_F(PluginStoreTest, PluginIsolation) {
    store_.set("plugin1", "key1", "value1");
    store_.set("plugin2", "key1", "value2");
    
    EXPECT_EQ(*store_.get("plugin1", "key1"), "value1");
    EXPECT_EQ(*store_.get("plugin2", "key1"), "value2");
}

// ============================================================================
// LuaEventBus Tests
// ============================================================================

class LuaEventBusTest : public ::testing::Test {
protected:
    LuaEventBus& bus_ = LuaEventBus::instance();
};

TEST_F(LuaEventBusTest, Subscribe) {
    int call_count = 0;
    auto handler = [&call_count](const std::string& event, const std::string& data) {
        call_count++;
    };
    
    auto sub_id = bus_.subscribe("test_event", handler);
    EXPECT_NE(sub_id, 0);
    
    bus_.unsubscribe(sub_id);
}

TEST_F(LuaEventBusTest, Emit) {
    int call_count = 0;
    std::string received_event;
    std::string received_data;
    
    auto handler = [&](const std::string& event, const std::string& data) {
        call_count++;
        received_event = event;
        received_data = data;
    };
    
    auto sub_id = bus_.subscribe("test_event", handler);
    bus_.emit("test_event", "{\"key\": \"value\"}");
    
    EXPECT_EQ(call_count, 1);
    EXPECT_EQ(received_event, "test_event");
    EXPECT_EQ(received_data, "{\"key\": \"value\"}");
    
    bus_.unsubscribe(sub_id);
}

TEST_F(LuaEventBusTest, MultipleSubscribers) {
    int call_count1 = 0;
    int call_count2 = 0;
    
    auto handler1 = [&](const std::string&, const std::string&) { call_count1++; };
    auto handler2 = [&](const std::string&, const std::string&) { call_count2++; };
    
    auto sub_id1 = bus_.subscribe("test_event", handler1);
    auto sub_id2 = bus_.subscribe("test_event", handler2);
    
    bus_.emit("test_event", "{}");
    
    EXPECT_EQ(call_count1, 1);
    EXPECT_EQ(call_count2, 1);
    
    bus_.unsubscribe(sub_id1);
    bus_.unsubscribe(sub_id2);
}

TEST_F(LuaEventBusTest, EventFiltering) {
    int call_count1 = 0;
    int call_count2 = 0;
    
    auto handler1 = [&](const std::string&, const std::string&) { call_count1++; };
    auto handler2 = [&](const std::string&, const std::string&) { call_count2++; };
    
    auto sub_id1 = bus_.subscribe("event1", handler1);
    auto sub_id2 = bus_.subscribe("event2", handler2);
    
    bus_.emit("event1", "{}");
    
    EXPECT_EQ(call_count1, 1);
    EXPECT_EQ(call_count2, 0);
    
    bus_.unsubscribe(sub_id1);
    bus_.unsubscribe(sub_id2);
}

TEST_F(LuaEventBusTest, Unsubscribe) {
    int call_count = 0;
    auto handler = [&](const std::string&, const std::string&) { call_count++; };
    
    auto sub_id = bus_.subscribe("test_event", handler);
    bus_.emit("test_event", "{}");
    EXPECT_EQ(call_count, 1);
    
    bus_.unsubscribe(sub_id);
    bus_.emit("test_event", "{}");
    EXPECT_EQ(call_count, 1); // Should not increase
}

// ============================================================================
// LuaCommandRegistry Tests
// ============================================================================

class LuaCommandRegistryTest : public ::testing::Test {
protected:
    LuaCommandRegistry& registry_ = LuaCommandRegistry::instance();
    
    void SetUp() override {
        // Unregister any test commands
        registry_.unregister_command("test_cmd");
        registry_.unregister_command("test_cmd2");
    }
};

TEST_F(LuaCommandRegistryTest, RegisterCommand) {
    LuaCommand cmd;
    cmd.name = "test_cmd";
    cmd.description = "Test command";
    cmd.handler = [](std::string_view account, std::string_view args) { return true; };
    
    EXPECT_TRUE(registry_.register_command(std::move(cmd)));
}

TEST_F(LuaCommandRegistryTest, RegisterDuplicate) {
    LuaCommand cmd1;
    cmd1.name = "test_cmd";
    cmd1.description = "Test command 1";
    cmd1.handler = [](std::string_view, std::string_view) { return true; };
    
    LuaCommand cmd2;
    cmd2.name = "test_cmd";
    cmd2.description = "Test command 2";
    cmd2.handler = [](std::string_view, std::string_view) { return true; };
    
    EXPECT_TRUE(registry_.register_command(std::move(cmd1)));
    EXPECT_FALSE(registry_.register_command(std::move(cmd2)));
}

TEST_F(LuaCommandRegistryTest, UnregisterCommand) {
    LuaCommand cmd;
    cmd.name = "test_cmd";
    cmd.description = "Test command";
    cmd.handler = [](std::string_view, std::string_view) { return true; };
    
    registry_.register_command(std::move(cmd));
    EXPECT_TRUE(registry_.unregister_command("test_cmd"));
    EXPECT_FALSE(registry_.unregister_command("test_cmd"));
}

TEST_F(LuaCommandRegistryTest, ListCommands) {
    LuaCommand cmd1;
    cmd1.name = "test_cmd1";
    cmd1.description = "Test command 1";
    cmd1.handler = [](std::string_view, std::string_view) { return true; };
    
    LuaCommand cmd2;
    cmd2.name = "test_cmd2";
    cmd2.description = "Test command 2";
    cmd2.handler = [](std::string_view, std::string_view) { return true; };
    
    registry_.register_command(std::move(cmd1));
    registry_.register_command(std::move(cmd2));
    
    auto cmds = registry_.list_commands();
    EXPECT_EQ(cmds.size(), 2);
}

TEST_F(LuaCommandRegistryTest, ExecuteCommand) {
    bool executed = false;
    std::string received_account;
    std::string received_args;
    
    LuaCommand cmd;
    cmd.name = "test_cmd";
    cmd.description = "Test command";
    cmd.handler = [&](std::string_view account, std::string_view args) {
        executed = true;
        received_account = std::string(account);
        received_args = std::string(args);
        return true;
    };
    
    registry_.register_command(std::move(cmd));
    
    EXPECT_TRUE(registry_.execute("player1", "test_cmd arg1 arg2"));
    EXPECT_TRUE(executed);
    EXPECT_EQ(received_account, "player1");
    EXPECT_EQ(received_args, "arg1 arg2");
}

TEST_F(LuaCommandRegistryTest, ExecuteNonexistent) {
    EXPECT_FALSE(registry_.execute("player1", "nonexistent_cmd"));
}

// ============================================================================
// SimpleHttpClient Tests
// ============================================================================

class SimpleHttpClientTest : public ::testing::Test {
};

TEST_F(SimpleHttpClientTest, GetValidUrl) {
    auto result = SimpleHttpClient::get("http://example.com/api");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->status_code, 200);
}

TEST_F(SimpleHttpClientTest, GetInvalidUrl) {
    auto result = SimpleHttpClient::get("not a valid url");
    EXPECT_FALSE(result.has_value());
}

TEST_F(SimpleHttpClientTest, PostValidUrl) {
    auto result = SimpleHttpClient::post("http://example.com/api", "{\"key\": \"value\"}");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->status_code, 200);
}

TEST_F(SimpleHttpClientTest, PostWithContentType) {
    auto result = SimpleHttpClient::post("http://example.com/api", "data", "text/plain");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->status_code, 200);
}

} // namespace pvpgn::infra::scripting::test
