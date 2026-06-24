// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// TOML Schema Versioning
//
// TomlSchemaValidator validates that a loaded TOML configuration string
// contains a `schema_version` field whose value is within the range
// [min_supported, max_supported].
//
// Usage:
//   auto result = TomlSchemaValidator::validate(toml_content);
//   if (!result.has_value()) {
//       log_error(result.error().message());
//       return;
//   }
//   // result.value() is the parsed schema_version integer
//
// See docs/operator/toml-schema-versioning.md for the versioning policy.

#include "core/result.hpp"
#include "core/error.hpp"
#include <cstdint>
#include <string_view>

namespace pvpgn::infra::config {

/// The minimum schema_version this build can load.
inline constexpr std::uint32_t kTomlSchemaMinSupported = 1;

/// The maximum schema_version this build understands.
/// Configs with a higher version are rejected (forward-incompatible).
inline constexpr std::uint32_t kTomlSchemaMaxSupported = 3;

/// Validates the `schema_version` field in a TOML configuration string.
///
/// The validator performs a lightweight line-by-line scan — it does NOT
/// require a full TOML parser.  It looks for a top-level assignment of the
/// form:
///
///   schema_version = <integer>
///
/// Rules:
///   - If the field is absent → error (missing schema_version)
///   - If the value is not a positive integer → error (malformed)
///   - If the value < kTomlSchemaMinSupported → error (too old)
///   - If the value > kTomlSchemaMaxSupported → error (too new / forward-compat)
///   - Otherwise → success, returns the parsed version number
class TomlSchemaValidator {
public:
    /// Validate the schema_version in the given TOML content.
    /// @param toml_content  Raw TOML text (e.g. contents of bnetd.toml)
    /// @returns  The parsed schema_version on success, or a descriptive Error.
    [[nodiscard]] static core::Result<std::uint32_t, core::Error>
    validate(std::string_view toml_content);

    /// Check whether a specific version integer is within the supported range.
    [[nodiscard]] static bool is_supported(std::uint32_t version) noexcept;
};

} // namespace pvpgn::infra::config
