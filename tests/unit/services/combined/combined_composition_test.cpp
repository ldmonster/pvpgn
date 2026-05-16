#include <gtest/gtest.h>
#include "services/combined/combined_composition.hpp"
#include "runtime/service_host.hpp"

namespace pvpgn::services::combined::test {

pvpgn::runtime::ServiceConfig make_config() {
    pvpgn::runtime::ServiceConfig cfg;
    cfg.service_name = "test-combined";
    return cfg;
}

TEST(CombinedCompositionTest, InitializeSucceeds) {
    CombinedComposition comp(make_config());
    auto result = comp.init(make_config());
    EXPECT_TRUE(result.has_value());
}

TEST(CombinedCompositionTest, StartAndStopSucceeds) {
    CombinedComposition comp(make_config());
    comp.init(make_config());
    auto start = comp.start();
    ASSERT_TRUE(start.has_value());
    EXPECT_TRUE(comp.is_healthy());

    comp.stop();
    EXPECT_FALSE(comp.is_healthy());
}

TEST(CombinedCompositionTest, RegistryHasAllServicesAfterInit) {
    CombinedComposition comp(make_config());
    comp.init(make_config());

    auto& reg = comp.registry();
    // After init, all three services should be registered
    EXPECT_FALSE(reg.lookup("bnetd").empty());
    EXPECT_FALSE(reg.lookup("d2cs").empty());
    EXPECT_FALSE(reg.lookup("d2dbs").empty());
}

TEST(CombinedCompositionTest, NotHealthyBeforeStart) {
    CombinedComposition comp(make_config());
    comp.init(make_config());
    EXPECT_FALSE(comp.is_healthy()); // Not started yet
}

} // namespace pvpgn::services::combined::test
