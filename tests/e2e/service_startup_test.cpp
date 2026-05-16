// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>
#include <memory>

#include "runtime/service_host.hpp"

using namespace pvpgn::runtime;

/// Mock service composition for testing
class MockServiceComposition : public IServiceComposition {
public:
    std::string service_name() const override {
        return "mock-service";
    }
    
    Result<void, std::string> init(const ServiceConfig& config) override {
        config_ = config;
        initialized_ = true;
        return Result<void, std::string>();
    }
    
    Result<void, std::string> start() override {
        started_ = true;
        return Result<void, std::string>();
    }
    
    void stop() override {
        stopped_ = true;
    }
    
    void shutdown() override {
        shutdown_ = true;
    }
    
    std::string status() const override {
        if (shutdown_) return "shutdown";
        if (stopped_) return "stopped";
        if (started_) return "running";
        if (initialized_) return "initialized";
        return "idle";
    }
    
    bool initialized_ = false;
    bool started_ = false;
    bool stopped_ = false;
    bool shutdown_ = false;
    ServiceConfig config_;
};

TEST_CASE("E2E: Service composition interface", "[e2e][service]") {
    auto composition = std::make_unique<MockServiceComposition>();
    
    REQUIRE(composition->service_name() == "mock-service");
    REQUIRE(composition->status() == "idle");
}

TEST_CASE("E2E: Service initialization", "[e2e][service]") {
    auto composition = std::make_unique<MockServiceComposition>();
    
    ServiceConfig config;
    config.service_name = "test-service";
    config.foreground = true;
    config.log_level = "debug";
    
    auto result = composition->init(config);
    REQUIRE(result);
    REQUIRE(composition->initialized_);
    REQUIRE(composition->config_.service_name == "test-service");
    REQUIRE(composition->config_.foreground);
    REQUIRE(composition->config_.log_level == "debug");
}

TEST_CASE("E2E: Service startup", "[e2e][service]") {
    auto composition = std::make_unique<MockServiceComposition>();
    
    ServiceConfig config;
    auto init_result = composition->init(config);
    REQUIRE(init_result);
    
    auto start_result = composition->start();
    REQUIRE(start_result);
    REQUIRE(composition->started_);
    REQUIRE(composition->status() == "running");
}

TEST_CASE("E2E: Service stop", "[e2e][service]") {
    auto composition = std::make_unique<MockServiceComposition>();
    
    ServiceConfig config;
    auto init_r = composition->init(config);
    REQUIRE(init_r);
    auto start_r = composition->start();
    REQUIRE(start_r);
    
    composition->stop();
    REQUIRE(composition->stopped_);
    REQUIRE(composition->status() == "stopped");
}

TEST_CASE("E2E: Service shutdown", "[e2e][service]") {
    auto composition = std::make_unique<MockServiceComposition>();
    
    ServiceConfig config;
    auto init_r = composition->init(config);
    REQUIRE(init_r);
    auto start_r = composition->start();
    REQUIRE(start_r);
    composition->stop();
    
    composition->shutdown();
    REQUIRE(composition->shutdown_);
    REQUIRE(composition->status() == "shutdown");
}

TEST_CASE("E2E: Full service lifecycle", "[e2e][service]") {
    auto composition = std::make_unique<MockServiceComposition>();
    
    // Initialize
    ServiceConfig config;
    config.service_name = "lifecycle-test";
    config.foreground = true;
    auto init_result = composition->init(config);
    REQUIRE(init_result);
    REQUIRE(composition->status() == "initialized");
    
    // Start
    auto start_result = composition->start();
    REQUIRE(start_result);
    REQUIRE(composition->status() == "running");
    
    // Stop
    composition->stop();
    REQUIRE(composition->status() == "stopped");
    
    // Shutdown
    composition->shutdown();
    REQUIRE(composition->status() == "shutdown");
}

TEST_CASE("E2E: Multiple service instances", "[e2e][service]") {
    auto service1 = std::make_unique<MockServiceComposition>();
    auto service2 = std::make_unique<MockServiceComposition>();
    
    ServiceConfig config1;
    config1.service_name = "service-1";
    auto init1_r = service1->init(config1);
    REQUIRE(init1_r);
    
    ServiceConfig config2;
    config2.service_name = "service-2";
    auto init2_r = service2->init(config2);
    REQUIRE(init2_r);
    
    REQUIRE(service1->config_.service_name == "service-1");
    REQUIRE(service2->config_.service_name == "service-2");
    
    auto start1_r = service1->start();
    REQUIRE(start1_r);
    auto start2_r = service2->start();
    REQUIRE(start2_r);
    
    REQUIRE(service1->status() == "running");
    REQUIRE(service2->status() == "running");
}
