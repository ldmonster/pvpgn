#include <catch2/catch_test_macros.hpp>

#include "infra/compat/platform.hpp"
#include "infra/compat/process.hpp"

namespace compat = pvpgn::v3::infra::compat;

TEST_CASE("platform booleans are mutually exclusive", "[infra][compat][platform]")
{
    const int hits = (compat::kIsWindows ? 1 : 0) + (compat::kIsLinux ? 1 : 0)
                     + (compat::kIsMacOS ? 1 : 0);
    REQUIRE(hits == 1);

    if constexpr (compat::kIsLinux || compat::kIsMacOS) {
        STATIC_REQUIRE(compat::kIsPosix);
    }
}

TEST_CASE("current_process_id returns the running PID", "[infra][compat][process]")
{
    const auto pid = compat::current_process_id();
    REQUIRE(pid != 0);
    REQUIRE(compat::current_process_id() == pid);
}

TEST_CASE("host_name returns a non-empty string", "[infra][compat][process]")
{
    const auto name = compat::host_name();
    REQUIRE(name.has_value());
    REQUIRE_FALSE(name->empty());
}

TEST_CASE("system_info populates sysname and machine", "[infra][compat][process]")
{
    const auto info = compat::system_info();
    REQUIRE(info.has_value());
    REQUIRE_FALSE(info->sysname.empty());
    REQUIRE_FALSE(info->machine.empty());
}
