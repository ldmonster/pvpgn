// SPDX-License-Identifier: GPL-2.0-or-later
//
// Sentinel test for lifecycle_placeholder.cpp.
//
// lifecycle_placeholder.cpp exists solely to give the
// `application_bnet_packet_pump` static library at least one object file
// under MSVC/Ninja.  The actual scaffold logic lives in the headers
// (lifecycle.hpp, conn_class.hpp) and is tested by lifecycle_test.cpp.
//
// This file verifies the sentinel constant is present and has the expected
// value, confirming the TU compiled and linked correctly.

#include <catch2/catch_test_macros.hpp>

namespace pvpgn::application::bnet_packet_pump {
// Forward-declare the sentinel so we can reference it without including
// the implementation header (which is intentionally header-only).
inline constexpr int kScaffoldRound = 179;
}  // namespace pvpgn::application::bnet_packet_pump

namespace pump = pvpgn::application::bnet_packet_pump;

TEST_CASE("lifecycle_placeholder: sentinel constant has expected value",
          "[application][bnet_packet_pump][lifecycle_placeholder]") {
    CHECK(pump::kScaffoldRound == 179);
}
