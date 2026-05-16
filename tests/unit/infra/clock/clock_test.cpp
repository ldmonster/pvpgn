// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include "infra/clock/legacy_clock_bridge.hpp"
#include "core/clock.hpp"
#include <chrono>
#include <thread>

namespace pvpgn::infra::clock::test {

class ClockTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test SystemClock returns reasonable time
TEST_F(ClockTest, SystemClockReturnsReasonableTime) {
    pvpgn::core::SystemClock clock;
    auto now = clock.now();
    
    // Get current time from system
    auto system_now = std::chrono::system_clock::now();
    
    // The difference should be very small (less than 1 second)
    auto diff = std::chrono::abs(system_now - now);
    EXPECT_LT(diff, std::chrono::seconds(1));
}

// Test SystemClock monotonic time is reasonable
TEST_F(ClockTest, SystemClockMonotonicTimeIsReasonable) {
    pvpgn::core::SystemClock clock;
    auto mono1 = clock.monotonic();
    
    // Sleep a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    auto mono2 = clock.monotonic();
    
    // mono2 should be >= mono1
    EXPECT_GE(mono2, mono1);
}

// Test MonotonicClock elapsed time is non-negative
TEST_F(ClockTest, MonotonicClockElapsedTimeIsNonNegative) {
    pvpgn::core::SystemClock clock;
    auto start = clock.monotonic();
    
    // Sleep a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    
    auto end = clock.monotonic();
    auto elapsed = end - start;
    
    // Elapsed should be non-negative
    EXPECT_GE(elapsed.count(), 0);
}

// Test ManualClock for deterministic testing
TEST_F(ClockTest, ManualClockAdvancesOnDemand) {
    auto start_time = std::chrono::system_clock::now();
    pvpgn::core::ManualClock clock(start_time);
    
    EXPECT_EQ(clock.now(), start_time);
    
    // Advance by 1 second
    clock.advance(std::chrono::seconds(1));
    
    auto expected = start_time + std::chrono::seconds(1);
    EXPECT_EQ(clock.now(), expected);
}

// Test ManualClock monotonic advances with system time
TEST_F(ClockTest, ManualClockMonotonicAdvancesWithSystemTime) {
    pvpgn::core::ManualClock clock;
    auto mono1 = clock.monotonic();
    
    clock.advance(std::chrono::milliseconds(100));
    
    auto mono2 = clock.monotonic();
    
    // mono2 should be 100ms after mono1
    auto diff = mono2 - mono1;
    EXPECT_EQ(diff, std::chrono::milliseconds(100));
}

// Test LegacyClockBridge compiles and returns a time_t
TEST_F(ClockTest, LegacyClockBridgeCompiles) {
    LegacyClockBridge bridge;
    
    // Should not crash
    auto now = bridge.now();
    auto mono = bridge.monotonic();
    
    // Both should be valid time points
    EXPECT_NE(now.time_since_epoch().count(), 0);
}

// Test that different clock implementations can be used polymorphically
TEST_F(ClockTest, PolymorphicClockUsage) {
    std::shared_ptr<pvpgn::core::IClock> system_clock = 
        std::make_shared<pvpgn::core::SystemClock>();
    
    std::shared_ptr<pvpgn::core::IClock> manual_clock = 
        std::make_shared<pvpgn::core::ManualClock>();
    
    // Both should work through the interface
    auto sys_now = system_clock->now();
    auto manual_now = manual_clock->now();
    
    EXPECT_NE(sys_now.time_since_epoch().count(), 0);
    EXPECT_EQ(manual_now.time_since_epoch().count(), 0);
}

} // namespace pvpgn::infra::clock::test
