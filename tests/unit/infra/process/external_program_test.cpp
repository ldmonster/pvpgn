// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "infra/process/external_program.hpp"

using namespace pvpgn::infra::process;

TEST_CASE("empty command rejected with InvalidArgument", "[infra][process]") {
    auto r = run_capture("");
    REQUIRE_FALSE(r);
    CHECK(r.error().code() == pvpgn::core::StatusCode::InvalidArgument);
}

#if !defined(_WIN32)
TEST_CASE("captures stdout of /bin/true (exit 0)", "[infra][process][posix]") {
    auto r = run_capture("true");
    REQUIRE(r);
    // On POSIX, exit_status==0 means normal exit 0.
    CHECK(r.value().exit_status == 0);
    CHECK(r.value().stdout_text.empty());
}

TEST_CASE("captures non-empty stdout via /bin/echo", "[infra][process][posix]") {
    // execlp(command, command, NULL) -- echo with no args prints "\n".
    auto r = run_capture("echo");
    REQUIRE(r);
    CHECK(r.value().stdout_text == "\n");
}

TEST_CASE("nonexistent command -> child exits 127, no output",
          "[infra][process][posix]") {
    auto r = run_capture("this-command-does-not-exist-xyz");
    REQUIRE(r);
    // Child execlp failed -> _Exit(127). Parent observes non-zero status.
    CHECK(r.value().exit_status != 0);
}
#else
TEST_CASE("Windows host returns Unimplemented", "[infra][process][win]") {
    auto r = run_capture("anything");
    REQUIRE_FALSE(r);
    CHECK(r.error().code() == pvpgn::core::StatusCode::Unimplemented);
}
#endif
