// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/infra/config/toml_validator_property_test.cpp
//
// No-panic property for the TOML config entry points. Parsing untrusted config
// (operators edit bnetd.toml by hand; a reload can feed half-written files)
// must NEVER throw or crash — it must always return a `core::Result`. This
// fuzzes `parse_server_config` and `TomlSchemaValidator::validate` with random
// bytes, randomly-assembled TOML tokens, and pathological inputs, asserting
// each call returns without throwing.
//
// Fixed seed → reproducible. No rapidcheck dependency. Under the ASan/UBSan CI
// jobs this doubles as a sanitizer target for the config parser.

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "infra/config/server_config.hpp"
#include "infra/config/toml_schema_validator.hpp"

using pvpgn::infra::config::TomlSchemaValidator;
using pvpgn::infra::config::parse_server_config;

namespace {

// TOML-ish fragments, including malformed ones, to assemble near-valid garbage
// that stresses the parser close to real syntax.
const char* const kTokens[] = {
    "[section]\n", "[[array]]\n", "key = ", "\"value\"", "= ", "123", "-4.5e9",
    "true", "false", "\n", "  ", "\t", "[", "]", "=", "\"", "'", "#comment\n",
    "[a.b.c]\n", "x = [1, 2, 3]\n", "schema_version = ", "{ inline = 1 }",
    "\\", "\x00", "é", "[[[[", "\"\"\"", "0x", "1_000", "", "name=\"",
};

struct Gen {
    std::mt19937 rng{0x70F1C0DEu};

    std::string random_bytes(std::size_t max_len = 200) {
        const std::size_t n = rng() % (max_len + 1);
        std::string s;
        s.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            s.push_back(static_cast<char>(static_cast<unsigned char>(rng())));
        }
        return s;
    }

    std::string random_toml_ish(std::size_t max_tokens = 40) {
        constexpr std::size_t kCount = sizeof(kTokens) / sizeof(kTokens[0]);
        const std::size_t n = rng() % (max_tokens + 1);
        std::string s;
        for (std::size_t i = 0; i < n; ++i) s += kTokens[rng() % kCount];
        return s;
    }
};

// Calling these must never throw, regardless of input.
void exercise(const std::string& input) {
    CHECK_NOTHROW((void)parse_server_config(input));
    CHECK_NOTHROW((void)TomlSchemaValidator::validate(input));
}

}  // namespace

TEST_CASE("TOML parsers: no panic on arbitrary bytes",
          "[infra][config][property]") {
    Gen g;
    for (int i = 0; i < 1500; ++i) {
        INFO("iteration " << i);
        exercise(g.random_bytes());
    }
}

TEST_CASE("TOML parsers: no panic on near-valid TOML garbage",
          "[infra][config][property]") {
    Gen g;
    for (int i = 0; i < 1500; ++i) {
        INFO("iteration " << i);
        exercise(g.random_toml_ish());
    }
}

TEST_CASE("TOML parsers: no panic on pathological inputs",
          "[infra][config][property]") {
    // Hand-picked stressors: deep nesting, unterminated constructs, huge keys.
    std::vector<std::string> cases = {
        "",
        "\n\n\n",
        std::string(100, '['),
        std::string(100, '"'),
        "[a" + std::string(500, '.') + "b]\n",
        "key = \"" + std::string(10000, 'x'),  // unterminated huge string
        std::string(5000, '='),
        "[[[[[[[[[[[[[[[[[[[[[[[[[[",
        "schema_version = 99999999999999999999999999",  // overflow-ish
        "key = " + std::string(2000, '0'),
        "\xff\xfe\x00\x01 garbage",
    };
    for (const auto& c : cases) {
        INFO("len=" << c.size());
        exercise(c);
    }
}

TEST_CASE("TOML parsers: a minimal valid config parses without error",
          "[infra][config][property]") {
    // Sanity: the no-panic harness still lets valid input through.
    const std::string ok = "schema_version = 1\n";
    auto v = TomlSchemaValidator::validate(ok);
    CHECK(v.has_value());
}
