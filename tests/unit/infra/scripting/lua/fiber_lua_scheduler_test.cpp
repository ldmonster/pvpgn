// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <boost/fiber/all.hpp>
#include "infra/scripting/lua/fiber_lua_scheduler.hpp"

namespace pvpgn::infra::scripting::lua::test {

TEST(FiberLuaSchedulerTest, RegisterReturnsUniqueIds) {
    FiberLuaScheduler sched;
    // Use nullptr for coro in unit test (we don't actually resume Lua)
    auto id1 = sched.register_async_op(nullptr);
    auto id2 = sched.register_async_op(nullptr);
    EXPECT_NE(id1, id2);
    EXPECT_EQ(sched.pending_count(), 2u);
}

TEST(FiberLuaSchedulerTest, YieldAndResumeRoundTrip) {
    FiberLuaScheduler sched;
    int result = -1;

    boost::fibers::fiber consumer([&] {
        auto op_id = sched.register_async_op(nullptr);
        result = sched.yield_for_async(op_id);
    });

    boost::fibers::fiber producer([&] {
        // Give consumer fiber a chance to register and yield
        boost::this_fiber::yield();
        sched.resume_coroutine(1, 42);
    });

    consumer.join();
    producer.join();
    EXPECT_EQ(result, 42);
}

TEST(FiberLuaSchedulerTest, CancelUnblocksFiber) {
    FiberLuaScheduler sched;
    int result = 999;

    boost::fibers::fiber f([&] {
        auto op_id = sched.register_async_op(nullptr);
        result = sched.yield_for_async(op_id);
    });

    boost::fibers::fiber canceller([&] {
        boost::this_fiber::yield();
        sched.cancel_async_op(1);
    });

    f.join();
    canceller.join();
    EXPECT_EQ(result, -1); // cancel returns -1
}

TEST(FiberLuaSchedulerTest, PendingCountTracksOps) {
    FiberLuaScheduler sched;
    EXPECT_EQ(sched.pending_count(), 0u);
    auto id = sched.register_async_op(nullptr);
    EXPECT_EQ(sched.pending_count(), 1u);
    sched.cancel_async_op(id);
    EXPECT_EQ(sched.pending_count(), 0u);
}

TEST(FiberLuaSchedulerTest, MultipleOperationsIndependent) {
    FiberLuaScheduler sched;
    int result1 = -1;
    int result2 = -1;

    boost::fibers::fiber consumer1([&] {
        auto op_id = sched.register_async_op(nullptr);
        result1 = sched.yield_for_async(op_id);
    });

    boost::fibers::fiber consumer2([&] {
        auto op_id = sched.register_async_op(nullptr);
        result2 = sched.yield_for_async(op_id);
    });

    boost::fibers::fiber producer([&] {
        // Give consumers a chance to register and yield
        boost::this_fiber::yield();
        boost::this_fiber::yield();
        sched.resume_coroutine(1, 100);
        sched.resume_coroutine(2, 200);
    });

    consumer1.join();
    consumer2.join();
    producer.join();
    EXPECT_EQ(result1, 100);
    EXPECT_EQ(result2, 200);
}

TEST(FiberLuaSchedulerTest, ResumeNonexistentOpDoesNotCrash) {
    FiberLuaScheduler sched;
    // Should not crash or throw
    sched.resume_coroutine(999, 42);
    EXPECT_EQ(sched.pending_count(), 0u);
}

TEST(FiberLuaSchedulerTest, CancelNonexistentOpDoesNotCrash) {
    FiberLuaScheduler sched;
    // Should not crash or throw
    sched.cancel_async_op(999);
    EXPECT_EQ(sched.pending_count(), 0u);
}

} // namespace pvpgn::infra::scripting::lua::test
