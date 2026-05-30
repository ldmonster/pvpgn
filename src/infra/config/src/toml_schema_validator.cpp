// SPDX-License-Identifier: GPL-2.0-or-later
// Plan 11 — §11.3  TOML Schema Versioning — validator implementation

#include "infra/config/toml_schema_validator.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <string>
#include <string_view>

namespace pvpgn::infra::config {

namespace {

/// Trim leading and trailing ASCII whitespace from a string_view.
std::string_view trim_sv(std::string_view s) noexcept {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

/// Returns true if the line is a comment or blank.
[[maybe_unused]] bool is_comment_or_blank(std::string_view line) noexcept {
    line = trim_sv(line);
    return line.empty() || line[0] == '#';
}

/// Returns true if the line is a TOML section header (e.g. "[storage]").
bool is_section_header(std::string_view line) noexcept {
    line = trim_sv(line);
    return !line.empty() && line[0] == '[';
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

core::Result<std::uint32_t, core::Error>
TomlSchemaValidator::validate(std::string_view toml_content) {
    // We scan only the top-level (pre-first-section) lines for schema_version.
    // Once we hit a [section] header we stop — schema_version must be at the
    // top of the file, not nested inside a section.

    bool found = false;
    std::uint32_t version = 0;

    std::string_view remaining = toml_content;

    while (!remaining.empty()) {
        // Extract one line
        auto newline_pos = remaining.find('\n');
        std::string_view line = (newline_pos == std::string_view::npos)
                                    ? remaining
                                    : remaining.substr(0, newline_pos);
        remaining = (newline_pos == std::string_view::npos)
                        ? std::string_view{}
                        : remaining.substr(newline_pos + 1);

        // Strip inline comment (everything after '#' that is not inside quotes)
        // Simple heuristic: find first '#' not preceded by a quote character.
        {
            bool in_string = false;
            for (std::size_t i = 0; i < line.size(); ++i) {
                if (line[i] == '"') { in_string = !in_string; }
                if (!in_string && line[i] == '#') {
                    line = line.substr(0, i);
                    break;
                }
            }
        }

        line = trim_sv(line);

        if (line.empty()) continue;

        // Stop scanning at the first section header
        if (is_section_header(line)) break;

        // Look for:  schema_version = <integer>
        constexpr std::string_view kKey = "schema_version";
        if (line.size() > kKey.size() &&
            line.substr(0, kKey.size()) == kKey)
        {
            std::string_view rest = trim_sv(line.substr(kKey.size()));
            if (!rest.empty() && rest[0] == '=') {
                rest = trim_sv(rest.substr(1));

                // Parse the integer value
                std::uint32_t parsed = 0;
                auto [ptr, ec] = std::from_chars(rest.data(),
                                                  rest.data() + rest.size(),
                                                  parsed);
                if (ec != std::errc{}) {
                    return core::fail(core::Error(
                        core::StatusCode::ConfigError,
                        "schema_version value is not a valid integer: " +
                            std::string(rest)));
                }

                // Ensure there is no trailing garbage (e.g. "3abc")
                std::string_view after = trim_sv(std::string_view(
                    ptr,
                    static_cast<std::size_t>(rest.data() + rest.size() - ptr)));
                if (!after.empty()) {
                    return core::fail(core::Error(
                        core::StatusCode::ConfigError,
                        "schema_version has unexpected trailing characters: " +
                            std::string(after)));
                }

                found = true;
                version = parsed;
                break;
            }
        }
    }

    if (!found) {
        return core::fail(core::Error(
            core::StatusCode::ConfigError,
            "Missing required top-level field 'schema_version' in TOML config. "
            "Add 'schema_version = " +
                std::to_string(kTomlSchemaMaxSupported) +
                "' at the top of the file. "
                "See docs/operator/toml-schema-versioning.md."));
    }

    if (version < kTomlSchemaMinSupported) {
        return core::fail(core::Error(
            core::StatusCode::SchemaMismatch,
            "TOML config schema_version " + std::to_string(version) +
                " is too old (minimum supported: " +
                std::to_string(kTomlSchemaMinSupported) + "). "
                "Please upgrade your configuration file."));
    }

    if (version > kTomlSchemaMaxSupported) {
        return core::fail(core::Error(
            core::StatusCode::SchemaMismatch,
            "TOML config schema_version " + std::to_string(version) +
                " is newer than this build supports (maximum: " +
                std::to_string(kTomlSchemaMaxSupported) + "). "
                "Please upgrade pvpgn or downgrade your configuration file."));
    }

    return version;
}

bool TomlSchemaValidator::is_supported(std::uint32_t version) noexcept {
    return version >= kTomlSchemaMinSupported && version <= kTomlSchemaMaxSupported;
}

} // namespace pvpgn::infra::config
