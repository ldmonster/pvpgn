// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for infra/compat/getopt.hpp
//
// Tests cover:
//   - Empty argv (just program name) — no options, no positionals
//   - Short flag option (-v)
//   - Short option with required argument (-o value, -ovalue)
//   - Long flag option (--verbose)
//   - Long option with required argument (--output=value, --output value)
//   - Mixed short and long options in one invocation
//   - Positional arguments (bare tokens)
//   - End-of-options separator (--) makes everything after positional
//   - Unknown short option returns error
//   - Unknown long option returns error
//   - Missing required argument for short option returns error
//   - Missing required argument for long option returns error
//   - Long option with inline = but spec says no_argument returns error
//   - Optional argument: present via --opt=val, absent via --opt
//   - Multiple flags set independently
//   - get() returns nullopt for absent option
//   - has() returns false for absent option
//   - usage() output contains registered option names
//   - parse_args() free function (vector overload)
//   - parse_args() free function (initializer_list overload)
//   - parse() is idempotent (second call resets state)

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "infra/compat/getopt.hpp"

namespace compat = pvpgn::v3::infra::compat;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// Build a null-terminated argv array from a vector of strings.
/// The returned vector owns the char* pointers (pointing into the strings).
/// The strings vector must outlive the returned argv.
std::vector<char*> make_argv(std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (auto& s : args) argv.push_back(s.data());
    argv.push_back(nullptr);
    return argv;
}

}  // namespace

// ---------------------------------------------------------------------------
// Empty argv
// ---------------------------------------------------------------------------

TEST_CASE("getopt: empty argv (just program name)", "[getopt]") {
    std::vector<std::string> args = {"prog"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('v', "verbose", "Enable verbose output");

    REQUIRE(parser.parse());
    CHECK_FALSE(parser.has("verbose"));
    CHECK_FALSE(parser.has("v"));
    CHECK(parser.positional_args().empty());
    CHECK(parser.error().empty());
}

// ---------------------------------------------------------------------------
// Short flag option
// ---------------------------------------------------------------------------

TEST_CASE("getopt: short flag -v sets has()", "[getopt]") {
    std::vector<std::string> args = {"prog", "-v"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('v', "verbose", "Enable verbose output");

    REQUIRE(parser.parse());
    CHECK(parser.has("verbose"));
    CHECK(parser.has("v"));
    CHECK(parser.positional_args().empty());
}

TEST_CASE("getopt: short flag -v get() returns empty string", "[getopt]") {
    std::vector<std::string> args = {"prog", "-v"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('v', "verbose", "Enable verbose output");

    REQUIRE(parser.parse());
    auto val = parser.get("verbose");
    REQUIRE(val.has_value());
    CHECK(val.value().empty());
}

// ---------------------------------------------------------------------------
// Short option with required argument
// ---------------------------------------------------------------------------

TEST_CASE("getopt: short option -o value (space-separated)", "[getopt]") {
    std::vector<std::string> args = {"prog", "-o", "outfile.txt"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('o', "output", "Output file", compat::ArgSpec::Arg::required);

    REQUIRE(parser.parse());
    REQUIRE(parser.has("output"));
    CHECK(parser.get("output").value() == "outfile.txt");
}

TEST_CASE("getopt: short option -ovalue (concatenated)", "[getopt]") {
    std::vector<std::string> args = {"prog", "-ooutfile.txt"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('o', "output", "Output file", compat::ArgSpec::Arg::required);

    REQUIRE(parser.parse());
    REQUIRE(parser.has("output"));
    CHECK(parser.get("output").value() == "outfile.txt");
}

// ---------------------------------------------------------------------------
// Long flag option
// ---------------------------------------------------------------------------

TEST_CASE("getopt: long flag --verbose sets has()", "[getopt]") {
    std::vector<std::string> args = {"prog", "--verbose"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('v', "verbose", "Enable verbose output");

    REQUIRE(parser.parse());
    CHECK(parser.has("verbose"));
    CHECK(parser.has("v"));
}

// ---------------------------------------------------------------------------
// Long option with required argument
// ---------------------------------------------------------------------------

TEST_CASE("getopt: long option --output=value (inline =)", "[getopt]") {
    std::vector<std::string> args = {"prog", "--output=result.txt"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('o', "output", "Output file", compat::ArgSpec::Arg::required);

    REQUIRE(parser.parse());
    REQUIRE(parser.has("output"));
    CHECK(parser.get("output").value() == "result.txt");
}

TEST_CASE("getopt: long option --output value (space-separated)", "[getopt]") {
    std::vector<std::string> args = {"prog", "--output", "result.txt"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('o', "output", "Output file", compat::ArgSpec::Arg::required);

    REQUIRE(parser.parse());
    REQUIRE(parser.has("output"));
    CHECK(parser.get("output").value() == "result.txt");
}

// ---------------------------------------------------------------------------
// Mixed short and long options
// ---------------------------------------------------------------------------

TEST_CASE("getopt: mixed short and long options", "[getopt]") {
    std::vector<std::string> args = {"prog", "-v", "--output=out.txt", "-n", "42"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('v', "verbose", "Verbose");
    parser.add_option('o', "output",  "Output file", compat::ArgSpec::Arg::required);
    parser.add_option('n', "count",   "Count",       compat::ArgSpec::Arg::required);

    REQUIRE(parser.parse());
    CHECK(parser.has("verbose"));
    CHECK(parser.get("output").value() == "out.txt");
    CHECK(parser.get("count").value()  == "42");
    CHECK(parser.positional_args().empty());
}

// ---------------------------------------------------------------------------
// Positional arguments
// ---------------------------------------------------------------------------

TEST_CASE("getopt: positional arguments are collected", "[getopt]") {
    std::vector<std::string> args = {"prog", "file1.txt", "file2.txt"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());

    REQUIRE(parser.parse());
    REQUIRE(parser.positional_args().size() == 2);
    CHECK(parser.positional_args()[0] == "file1.txt");
    CHECK(parser.positional_args()[1] == "file2.txt");
}

TEST_CASE("getopt: options and positionals mixed", "[getopt]") {
    std::vector<std::string> args = {"prog", "-v", "file1.txt", "-o", "out.txt", "file2.txt"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('v', "verbose", "Verbose");
    parser.add_option('o', "output",  "Output", compat::ArgSpec::Arg::required);

    REQUIRE(parser.parse());
    CHECK(parser.has("verbose"));
    CHECK(parser.get("output").value() == "out.txt");
    REQUIRE(parser.positional_args().size() == 2);
    CHECK(parser.positional_args()[0] == "file1.txt");
    CHECK(parser.positional_args()[1] == "file2.txt");
}

// ---------------------------------------------------------------------------
// End-of-options separator (--)
// ---------------------------------------------------------------------------

TEST_CASE("getopt: -- makes everything after it positional", "[getopt]") {
    std::vector<std::string> args = {"prog", "-v", "--", "--not-an-option", "-x", "file.txt"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('v', "verbose", "Verbose");

    REQUIRE(parser.parse());
    CHECK(parser.has("verbose"));
    REQUIRE(parser.positional_args().size() == 3);
    CHECK(parser.positional_args()[0] == "--not-an-option");
    CHECK(parser.positional_args()[1] == "-x");
    CHECK(parser.positional_args()[2] == "file.txt");
}

TEST_CASE("getopt: bare -- with nothing after it is fine", "[getopt]") {
    std::vector<std::string> args = {"prog", "--"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());

    REQUIRE(parser.parse());
    CHECK(parser.positional_args().empty());
}

// ---------------------------------------------------------------------------
// Unknown option errors
// ---------------------------------------------------------------------------

TEST_CASE("getopt: unknown short option returns error", "[getopt]") {
    std::vector<std::string> args = {"prog", "-z"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('v', "verbose", "Verbose");

    CHECK_FALSE(parser.parse());
    CHECK_FALSE(parser.error().empty());
    CHECK(parser.error().find("-z") != std::string_view::npos);
}

TEST_CASE("getopt: unknown long option returns error", "[getopt]") {
    std::vector<std::string> args = {"prog", "--unknown"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('v', "verbose", "Verbose");

    CHECK_FALSE(parser.parse());
    CHECK_FALSE(parser.error().empty());
    CHECK(parser.error().find("unknown") != std::string_view::npos);
}

// ---------------------------------------------------------------------------
// Missing required argument errors
// ---------------------------------------------------------------------------

TEST_CASE("getopt: missing required argument for short option returns error", "[getopt]") {
    std::vector<std::string> args = {"prog", "-o"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('o', "output", "Output file", compat::ArgSpec::Arg::required);

    CHECK_FALSE(parser.parse());
    CHECK_FALSE(parser.error().empty());
}

TEST_CASE("getopt: missing required argument for long option returns error", "[getopt]") {
    std::vector<std::string> args = {"prog", "--output"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('o', "output", "Output file", compat::ArgSpec::Arg::required);

    CHECK_FALSE(parser.parse());
    CHECK_FALSE(parser.error().empty());
}

// ---------------------------------------------------------------------------
// Long option with = but spec says no_argument
// ---------------------------------------------------------------------------

TEST_CASE("getopt: long flag with inline = value returns error", "[getopt]") {
    std::vector<std::string> args = {"prog", "--verbose=yes"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('v', "verbose", "Verbose");  // no_argument

    CHECK_FALSE(parser.parse());
    CHECK_FALSE(parser.error().empty());
}

// ---------------------------------------------------------------------------
// Optional argument
// ---------------------------------------------------------------------------

TEST_CASE("getopt: optional argument present via --opt=val", "[getopt]") {
    std::vector<std::string> args = {"prog", "--level=3"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('l', "level", "Log level", compat::ArgSpec::Arg::optional);

    REQUIRE(parser.parse());
    REQUIRE(parser.has("level"));
    CHECK(parser.get("level").value() == "3");
}

TEST_CASE("getopt: optional argument absent via --opt (no =)", "[getopt]") {
    std::vector<std::string> args = {"prog", "--level"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('l', "level", "Log level", compat::ArgSpec::Arg::optional);

    REQUIRE(parser.parse());
    REQUIRE(parser.has("level"));
    CHECK(parser.get("level").value().empty());
}

// ---------------------------------------------------------------------------
// Multiple flags set independently
// ---------------------------------------------------------------------------

TEST_CASE("getopt: multiple flags set independently", "[getopt]") {
    std::vector<std::string> args = {"prog", "-v", "-d"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('v', "verbose", "Verbose");
    parser.add_option('d', "debug",   "Debug");
    parser.add_option('q', "quiet",   "Quiet");

    REQUIRE(parser.parse());
    CHECK(parser.has("verbose"));
    CHECK(parser.has("debug"));
    CHECK_FALSE(parser.has("quiet"));
}

// ---------------------------------------------------------------------------
// get() / has() for absent options
// ---------------------------------------------------------------------------

TEST_CASE("getopt: get() returns nullopt for absent option", "[getopt]") {
    std::vector<std::string> args = {"prog"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('v', "verbose", "Verbose");

    REQUIRE(parser.parse());
    CHECK_FALSE(parser.get("verbose").has_value());
    CHECK_FALSE(parser.get("v").has_value());
    CHECK_FALSE(parser.get("nonexistent").has_value());
}

TEST_CASE("getopt: has() returns false for absent option", "[getopt]") {
    std::vector<std::string> args = {"prog"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('v', "verbose", "Verbose");

    REQUIRE(parser.parse());
    CHECK_FALSE(parser.has("verbose"));
    CHECK_FALSE(parser.has("v"));
}

// ---------------------------------------------------------------------------
// usage() output
// ---------------------------------------------------------------------------

TEST_CASE("getopt: usage() contains registered option names", "[getopt]") {
    std::vector<std::string> args = {"prog"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('v', "verbose", "Enable verbose output");
    parser.add_option('o', "output",  "Output file", compat::ArgSpec::Arg::required);
    parser.add_option('\0', "config", "Config file",  compat::ArgSpec::Arg::required);

    std::string help = parser.usage("prog");
    CHECK(help.find("verbose") != std::string::npos);
    CHECK(help.find("output")  != std::string::npos);
    CHECK(help.find("config")  != std::string::npos);
    CHECK(help.find("-v")      != std::string::npos);
    CHECK(help.find("-o")      != std::string::npos);
    CHECK(help.find("Enable verbose output") != std::string::npos);
}

TEST_CASE("getopt: usage() contains program name", "[getopt]") {
    std::vector<std::string> args = {"prog"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    std::string help = parser.usage("my_program");
    CHECK(help.find("my_program") != std::string::npos);
}

// ---------------------------------------------------------------------------
// parse_args() free function
// ---------------------------------------------------------------------------

TEST_CASE("getopt: parse_args() vector overload", "[getopt]") {
    std::vector<std::string> args = {"prog", "-v", "--output=out.txt"};
    auto argv = make_argv(args);

    std::vector<compat::ArgSpec> specs = {
        {'v', "verbose", "Verbose",     compat::ArgSpec::Arg::none},
        {'o', "output",  "Output file", compat::ArgSpec::Arg::required},
    };

    auto result = compat::parse_args(static_cast<int>(args.size()), argv.data(), specs);
    REQUIRE(result.ok());
    CHECK(result.has("verbose"));
    CHECK(result.get("output").value() == "out.txt");
}

TEST_CASE("getopt: parse_args() initializer_list overload", "[getopt]") {
    std::vector<std::string> args = {"prog", "--debug", "-n", "5"};
    auto argv = make_argv(args);

    auto result = compat::parse_args(static_cast<int>(args.size()), argv.data(), {
        compat::ArgSpec{'d', "debug", "Debug mode",  compat::ArgSpec::Arg::none},
        compat::ArgSpec{'n', "count", "Item count",  compat::ArgSpec::Arg::required},
    });

    REQUIRE(result.ok());
    CHECK(result.has("debug"));
    CHECK(result.get("count").value() == "5");
}

TEST_CASE("getopt: parse_args() returns error on unknown option", "[getopt]") {
    std::vector<std::string> args = {"prog", "--unknown"};
    auto argv = make_argv(args);

    auto result = compat::parse_args(static_cast<int>(args.size()), argv.data(), {
        compat::ArgSpec{'v', "verbose", "Verbose", compat::ArgSpec::Arg::none},
    });

    CHECK_FALSE(result.ok());
    CHECK_FALSE(result.error.empty());
}

// ---------------------------------------------------------------------------
// parse() idempotency
// ---------------------------------------------------------------------------

TEST_CASE("getopt: parse() is idempotent — second call resets state", "[getopt]") {
    std::vector<std::string> args = {"prog", "-v"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('v', "verbose", "Verbose");

    REQUIRE(parser.parse());
    CHECK(parser.has("verbose"));

    // Second parse should produce the same result.
    REQUIRE(parser.parse());
    CHECK(parser.has("verbose"));
    CHECK(parser.error().empty());
}

TEST_CASE("getopt: parse() resets previous error on retry", "[getopt]") {
    // First parse with bad args.
    std::vector<std::string> bad_args = {"prog", "--unknown"};
    auto bad_argv = make_argv(bad_args);

    compat::CommandLineParser parser(static_cast<int>(bad_args.size()), bad_argv.data());
    parser.add_option('v', "verbose", "Verbose");

    CHECK_FALSE(parser.parse());
    CHECK_FALSE(parser.error().empty());

    // Re-parse with the same (bad) args — error should still be set.
    CHECK_FALSE(parser.parse());
    CHECK_FALSE(parser.error().empty());
}

// ---------------------------------------------------------------------------
// Long-only option (no short name)
// ---------------------------------------------------------------------------

TEST_CASE("getopt: long-only option (short_name = '\\0')", "[getopt]") {
    std::vector<std::string> args = {"prog", "--config", "pvpgn.conf"};
    auto argv = make_argv(args);

    compat::CommandLineParser parser(static_cast<int>(args.size()), argv.data());
    parser.add_option('\0', "config", "Config file", compat::ArgSpec::Arg::required);

    REQUIRE(parser.parse());
    REQUIRE(parser.has("config"));
    CHECK(parser.get("config").value() == "pvpgn.conf");
}

// ---------------------------------------------------------------------------
// ParseResult::get() and has() member functions
// ---------------------------------------------------------------------------

TEST_CASE("getopt: ParseResult::get() and has() work correctly", "[getopt]") {
    std::vector<std::string> args = {"prog", "-v", "-o", "file.txt"};
    auto argv = make_argv(args);

    auto result = compat::parse_args(static_cast<int>(args.size()), argv.data(), {
        compat::ArgSpec{'v', "verbose", "Verbose",     compat::ArgSpec::Arg::none},
        compat::ArgSpec{'o', "output",  "Output file", compat::ArgSpec::Arg::required},
    });

    REQUIRE(result.ok());
    CHECK(result.has("verbose"));
    CHECK(result.has("output"));
    CHECK_FALSE(result.has("nonexistent"));
    CHECK(result.get("output").value() == "file.txt");
    CHECK_FALSE(result.get("nonexistent").has_value());
}
