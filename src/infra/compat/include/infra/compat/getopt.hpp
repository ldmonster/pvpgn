// SPDX-License-Identifier: GPL-2.0-or-later
//
// Modern C++20 command-line argument parser.
//
// The legacy code was a 855-line portable reimplementation of GNU getopt /
// getopt_long, compiled only when the host system lacked `getopt` (i.e. on
// Windows or very old POSIX systems).  It exposed the classic C globals
// `optarg`, `optind`, `opterr`, `optopt` and the `struct option` type inside
// the `pvpgn` namespace.
//
// This replaces all of that with a self-contained, header-only
// C++20 argument parser.  No external dependencies are required.
//
// ## Types
//
//   `ArgSpec`            — describes one registered option (short + long name,
//                          description, whether it takes an argument)
//   `ParseResult`        — result of a completed parse: option values, flags,
//                          positional arguments, and an optional error message
//   `CommandLineParser`  — stateful parser; register options then call parse()
//
// ## CommandLineParser API
//
//   Constructor:  `CommandLineParser(int argc, char* argv[])`
//   Registration: `add_option(short_name, long_name, description, has_arg)`
//   Parsing:      `parse()` → `bool`  (false on error; call `error()` for msg)
//   Query:        `get(name)` → `std::optional<std::string>`
//                 `has(name)` → `bool`
//                 `positional_args()` → `const std::vector<std::string>&`
//                 `error()` → `std::string_view`
//   Help:         `usage(program_name)` → `std::string`
//
// ## Free function API (legacy-compatible)
//
//   `parse_args(argc, argv, options)` → `ParseResult`
//
// ## Design decisions
//
//   - No exceptions — errors are reported via `std::optional` / `ParseResult::error`.
//   - `[[nodiscard]]` on all factory / query functions.
//   - Header-only — no `.cpp` file needed.
//   - Supports:
//       short options:  `-v`, `-o value`, `-ovalue`
//       long options:   `--verbose`, `--output=value`, `--output value`
//       end-of-options: `--` (all subsequent tokens are positional)
//       unknown option  → error
//       missing arg     → error
//   - `has_arg` semantics mirror POSIX `getopt_long`:
//       `ArgSpec::Arg::none`     — flag only, no argument
//       `ArgSpec::Arg::required` — option requires an argument
//       `ArgSpec::Arg::optional` — argument is optional (only `--opt=val` form)
//   - Option lookup by either short char or long name string.
//   - `parse()` is idempotent: calling it again resets and re-parses.

#pragma once

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pvpgn::v3::infra::compat {

// ---------------------------------------------------------------------------
// ArgSpec — describes one registered option
// ---------------------------------------------------------------------------

/// Describes a single command-line option that the parser should recognise.
struct ArgSpec {
    /// Whether the option takes an argument.
    enum class Arg : int {
        none     = 0,  ///< Flag only — no argument follows.
        required = 1,  ///< An argument is required (e.g. `--output file`).
        optional = 2,  ///< An argument is optional (only `--opt=val` form).
    };

    /// Single-character short name (e.g. `'v'`).  Use `'\0'` for long-only.
    char        short_name{'\0'};
    /// Long name without leading `--` (e.g. `"verbose"`).  May be empty.
    std::string long_name;
    /// Human-readable description for `usage()` output.
    std::string description;
    /// Whether this option takes an argument.
    Arg         has_arg{Arg::none};
};

// ---------------------------------------------------------------------------
// ParseResult — result of a completed parse
// ---------------------------------------------------------------------------

/// Result returned by `parse_args()` and accessible via `CommandLineParser`
/// after a successful `parse()` call.
struct ParseResult {
    /// Map from canonical option name (long name if available, else short
    /// char as a one-character string) to the option's value.
    /// For flag options the value is an empty string.
    std::unordered_map<std::string, std::string> options;

    /// Positional arguments (non-option tokens, and everything after `--`).
    std::vector<std::string> positional;

    /// Non-empty when parsing failed.
    std::string error;

    /// Returns true when parsing succeeded (no error).
    [[nodiscard]] bool ok() const noexcept { return error.empty(); }

    /// Returns the value for `name`, or `std::nullopt` if not present.
    [[nodiscard]] std::optional<std::string> get(std::string_view name) const {
        auto it = options.find(std::string(name));
        if (it == options.end()) return std::nullopt;
        return it->second;
    }

    /// Returns true if `name` was present on the command line.
    [[nodiscard]] bool has(std::string_view name) const noexcept {
        return options.count(std::string(name)) != 0;
    }
};

// ---------------------------------------------------------------------------
// CommandLineParser — stateful C++20 argument parser
// ---------------------------------------------------------------------------

/// Stateful command-line argument parser.
///
/// Usage:
/// ```cpp
/// CommandLineParser parser(argc, argv);
/// parser.add_option('v', "verbose", "Enable verbose output");
/// parser.add_option('o', "output",  "Output file",  ArgSpec::Arg::required);
/// if (!parser.parse()) {
///     std::cerr << parser.error() << '\n';
///     std::cerr << parser.usage(argv[0]);
///     return 1;
/// }
/// bool verbose = parser.has("verbose");
/// auto out     = parser.get("output");  // std::optional<std::string>
/// ```
class CommandLineParser {
public:
    /// Construct with the raw `argc`/`argv` from `main()`.
    /// Does NOT parse yet — call `parse()` explicitly.
    CommandLineParser(int argc, char* const* argv)
        : argc_(argc), argv_(argv) {}

    // Non-copyable, movable.
    CommandLineParser(const CommandLineParser&)            = delete;
    CommandLineParser& operator=(const CommandLineParser&) = delete;
    CommandLineParser(CommandLineParser&&)                 = default;
    CommandLineParser& operator=(CommandLineParser&&)      = default;

    // -----------------------------------------------------------------------
    // Registration
    // -----------------------------------------------------------------------

    /// Register an option.
    ///
    /// @param short_name  Single character (e.g. `'v'`), or `'\0'` for long-only.
    /// @param long_name   Long name without `--` (e.g. `"verbose"`), or empty.
    /// @param description Human-readable description for `usage()`.
    /// @param has_arg     Whether the option takes an argument.
    void add_option(char short_name,
                    std::string_view long_name,
                    std::string_view description,
                    ArgSpec::Arg has_arg = ArgSpec::Arg::none)
    {
        specs_.push_back(ArgSpec{
            short_name,
            std::string(long_name),
            std::string(description),
            has_arg,
        });
    }

    // -----------------------------------------------------------------------
    // Parsing
    // -----------------------------------------------------------------------

    /// Parse `argc_`/`argv_` according to the registered options.
    ///
    /// Resets any previous parse result before starting.
    ///
    /// @return `true` on success, `false` on error (call `error()` for details).
    [[nodiscard]] bool parse() {
        // Reset state.
        result_ = ParseResult{};

        if (argc_ <= 0 || argv_ == nullptr) return true;  // nothing to parse

        bool end_of_options = false;

        for (int i = 1; i < argc_; ++i) {
            std::string_view token(argv_[i]);

            if (end_of_options) {
                result_.positional.emplace_back(token);
                continue;
            }

            if (token == "--") {
                end_of_options = true;
                continue;
            }

            if (token.size() >= 2 && token[0] == '-' && token[1] == '-') {
                // Long option: --name  or  --name=value
                if (!parse_long(token.substr(2), i)) return false;
            } else if (token.size() >= 2 && token[0] == '-') {
                // Short option(s): -v  or  -ovalue  or  -o value
                if (!parse_short(token.substr(1), i)) return false;
            } else {
                result_.positional.emplace_back(token);
            }
        }

        return true;
    }

    // -----------------------------------------------------------------------
    // Query
    // -----------------------------------------------------------------------

    /// Returns the value for option `name` (long name preferred, else short
    /// char as a one-character string), or `std::nullopt` if not present.
    [[nodiscard]] std::optional<std::string> get(std::string_view name) const {
        return result_.get(name);
    }

    /// Returns `true` if option `name` was present on the command line.
    [[nodiscard]] bool has(std::string_view name) const noexcept {
        return result_.has(name);
    }

    /// Returns the list of positional (non-option) arguments.
    [[nodiscard]] const std::vector<std::string>& positional_args() const noexcept {
        return result_.positional;
    }

    /// Returns the error message from the last `parse()` call, or an empty
    /// string_view if parsing succeeded.
    [[nodiscard]] std::string_view error() const noexcept {
        return result_.error;
    }

    /// Returns the full `ParseResult` from the last `parse()` call.
    [[nodiscard]] const ParseResult& result() const noexcept {
        return result_;
    }

    // -----------------------------------------------------------------------
    // Help
    // -----------------------------------------------------------------------

    /// Generate a usage/help string listing all registered options.
    ///
    /// @param program_name  Typically `argv[0]`.
    [[nodiscard]] std::string usage(std::string_view program_name) const {
        std::ostringstream oss;
        oss << "Usage: " << program_name << " [options] [--] [args...]\n\nOptions:\n";

        for (const auto& spec : specs_) {
            oss << "  ";
            if (spec.short_name != '\0') {
                oss << '-' << spec.short_name;
                if (!spec.long_name.empty()) oss << ", ";
            } else {
                oss << "    ";
            }
            if (!spec.long_name.empty()) {
                oss << "--" << spec.long_name;
            }
            switch (spec.has_arg) {
                case ArgSpec::Arg::required:
                    oss << " <arg>";
                    break;
                case ArgSpec::Arg::optional:
                    oss << "[=arg]";
                    break;
                case ArgSpec::Arg::none:
                    break;
            }
            if (!spec.description.empty()) {
                oss << "\n      " << spec.description;
            }
            oss << '\n';
        }
        return oss.str();
    }

private:
    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    /// Find a spec by long name.
    [[nodiscard]] const ArgSpec* find_long(std::string_view name) const noexcept {
        for (const auto& s : specs_) {
            if (s.long_name == name) return &s;
        }
        return nullptr;
    }

    /// Find a spec by short character.
    [[nodiscard]] const ArgSpec* find_short(char c) const noexcept {
        for (const auto& s : specs_) {
            if (s.short_name == c) return &s;
        }
        return nullptr;
    }

    /// Canonical key for a spec (long name if available, else short char).
    [[nodiscard]] static std::string canonical_key(const ArgSpec& spec) {
        if (!spec.long_name.empty()) return spec.long_name;
        return std::string(1, spec.short_name);
    }

    /// Record an option value in the result map.
    void record(const ArgSpec& spec, std::string value) {
        result_.options[canonical_key(spec)] = std::move(value);
        // Also record under the short-char key so both lookups work.
        if (spec.short_name != '\0' && !spec.long_name.empty()) {
            result_.options[std::string(1, spec.short_name)] = result_.options[canonical_key(spec)];
        }
    }

    /// Parse a long option token (everything after `--`).
    /// `i` is the current argv index (may be advanced to consume the next token).
    bool parse_long(std::string_view token, int& i) {
        // Split on '=' if present.
        auto eq = token.find('=');
        std::string_view name  = (eq != std::string_view::npos) ? token.substr(0, eq) : token;
        std::string_view value = (eq != std::string_view::npos) ? token.substr(eq + 1) : "";
        bool has_inline_value  = (eq != std::string_view::npos);

        const ArgSpec* spec = find_long(name);
        if (spec == nullptr) {
            result_.error = "unknown option: --";
            result_.error += name;
            return false;
        }

        switch (spec->has_arg) {
            case ArgSpec::Arg::none:
                if (has_inline_value) {
                    result_.error = "option does not take an argument: --";
                    result_.error += name;
                    return false;
                }
                record(*spec, "");
                break;

            case ArgSpec::Arg::required:
                if (has_inline_value) {
                    record(*spec, std::string(value));
                } else {
                    // Consume next token.
                    if (i + 1 >= argc_) {
                        result_.error = "option requires an argument: --";
                        result_.error += name;
                        return false;
                    }
                    record(*spec, std::string(argv_[++i]));
                }
                break;

            case ArgSpec::Arg::optional:
                // Optional argument only via `--opt=val` form.
                record(*spec, has_inline_value ? std::string(value) : "");
                break;
        }
        return true;
    }

    /// Parse one or more short option characters from `token` (everything
    /// after the leading `-`).  `i` may be advanced to consume the next token.
    bool parse_short(std::string_view token, int& i) {
        for (std::size_t pos = 0; pos < token.size(); ++pos) {
            char c = token[pos];
            const ArgSpec* spec = find_short(c);
            if (spec == nullptr) {
                result_.error = "unknown option: -";
                result_.error += c;
                return false;
            }

            switch (spec->has_arg) {
                case ArgSpec::Arg::none:
                    record(*spec, "");
                    break;

                case ArgSpec::Arg::required: {
                    // Remainder of token is the value (e.g. `-ofile`),
                    // or consume the next token.
                    std::string_view remainder = token.substr(pos + 1);
                    if (!remainder.empty()) {
                        record(*spec, std::string(remainder));
                        return true;  // consumed rest of token
                    }
                    if (i + 1 >= argc_) {
                        result_.error = "option requires an argument: -";
                        result_.error += c;
                        return false;
                    }
                    record(*spec, std::string(argv_[++i]));
                    return true;  // consumed next token
                }

                case ArgSpec::Arg::optional: {
                    // Optional: remainder of token is the value if present.
                    std::string_view remainder = token.substr(pos + 1);
                    record(*spec, std::string(remainder));
                    return true;
                }
            }
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // Data members
    // -----------------------------------------------------------------------

    int           argc_{0};
    char* const*  argv_{nullptr};
    std::vector<ArgSpec> specs_;
    ParseResult   result_;
};

// ---------------------------------------------------------------------------
// Free function API — legacy-compatible
// ---------------------------------------------------------------------------

/// Parse `argc`/`argv` using the provided option specifications.
///
/// This is a convenience wrapper around `CommandLineParser` for callers that
/// prefer a functional style.
///
/// @param argc     Argument count (as received by `main`).
/// @param argv     Argument vector (as received by `main`).
/// @param options  Registered option specifications.
/// @return         A `ParseResult` with `.ok()` true on success.
[[nodiscard]] inline ParseResult parse_args(int argc,
                                            char* const* argv,
                                            const std::vector<ArgSpec>& options)
{
    CommandLineParser parser(argc, argv);
    for (const auto& spec : options) {
        parser.add_option(spec.short_name, spec.long_name,
                          spec.description, spec.has_arg);
    }
    if (!parser.parse()) {
        return parser.result();
    }
    return parser.result();
}

/// Overload accepting an initializer list of `ArgSpec`.
[[nodiscard]] inline ParseResult parse_args(int argc,
                                            char* const* argv,
                                            std::initializer_list<ArgSpec> options)
{
    return parse_args(argc, argv, std::vector<ArgSpec>(options));
}

}  // namespace pvpgn::v3::infra::compat
