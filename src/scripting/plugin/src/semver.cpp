// SPDX-License-Identifier: GPL-2.0-or-later
#include "scripting/plugin/semver.hpp"
#include <vector>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <string>

namespace pvpgn::scripting::plugin {

namespace {

/// Split a string by a delimiter
std::vector<std::string> split(std::string_view str, char delim) {
    std::vector<std::string> result;
    std::string current;
    for (char c : str) {
        if (c == delim) {
            result.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    result.push_back(current);
    return result;
}

/// Check if a string is all digits
bool is_digits(std::string_view str) {
    return !str.empty() && std::all_of(str.begin(), str.end(), [](char c) {
        return std::isdigit(static_cast<unsigned char>(c));
    });
}

/// Parse a single prerelease identifier
/// Returns (numeric_value, alphanumeric_string, is_numeric)
struct PrereleaseId {
    uint32_t numeric_value = 0;
    std::string alpha_value;
    bool is_numeric = false;

    static PrereleaseId parse(std::string_view id) {
        PrereleaseId result;
        if (is_digits(id)) {
            result.is_numeric = true;
            result.numeric_value = std::stoul(std::string(id));
        } else {
            result.is_numeric = false;
            result.alpha_value = std::string(id);
        }
        return result;
    }

    int compare(const PrereleaseId& other) const noexcept {
        // Numeric identifiers are compared numerically
        if (is_numeric && other.is_numeric) {
            if (numeric_value < other.numeric_value) return -1;
            if (numeric_value > other.numeric_value) return 1;
            return 0;
        }
        // Numeric identifiers always have lower precedence than non-numeric
        if (is_numeric && !other.is_numeric) return -1;
        if (!is_numeric && other.is_numeric) return 1;
        // Alphanumeric identifiers are compared lexically
        return alpha_value.compare(other.alpha_value);
    }
};

} // namespace

core::Result<SemVer, core::Error> SemVer::parse(std::string_view version_str) {
    if (version_str.empty()) {
        return core::fail(core::Error(core::StatusCode::InvalidArgument, "Empty version string"));
    }

    SemVer result;
    std::string version = std::string(version_str);

    // Extract build metadata (after +)
    size_t build_pos = version.find('+');
    if (build_pos != std::string::npos) {
        result.build_meta = version.substr(build_pos + 1);
        version = version.substr(0, build_pos);
    }

    // Extract prerelease (after -)
    size_t pre_pos = version.find('-');
    if (pre_pos != std::string::npos) {
        result.prerelease = version.substr(pre_pos + 1);
        version = version.substr(0, pre_pos);
    }

    // Parse major.minor.patch
    auto parts = split(version, '.');
    if (parts.size() != 3) {
        return core::fail(core::Error(core::StatusCode::InvalidArgument,
            "Invalid semver format: expected MAJOR.MINOR.PATCH"));
    }

    if (!is_digits(parts[0]) || !is_digits(parts[1]) || !is_digits(parts[2])) {
        return core::fail(core::Error(core::StatusCode::InvalidArgument,
            "Invalid semver: major, minor, patch must be non-negative integers"));
    }

    try {
        result.major = std::stoul(parts[0]);
        result.minor = std::stoul(parts[1]);
        result.patch = std::stoul(parts[2]);
    } catch (...) {
        return core::fail(core::Error(core::StatusCode::InvalidArgument,
            "Invalid semver: version numbers out of range"));
    }

    return result;
}

std::string SemVer::to_string() const {
    std::string result = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    if (!prerelease.empty()) {
        result += "-" + prerelease;
    }
    if (!build_meta.empty()) {
        result += "+" + build_meta;
    }
    return result;
}

bool SemVer::operator<(const SemVer& other) const noexcept {
    // Compare major.minor.patch
    if (major != other.major) return major < other.major;
    if (minor != other.minor) return minor < other.minor;
    if (patch != other.patch) return patch < other.patch;

    // When major.minor.patch are equal, prerelease matters
    // Empty prerelease (release) > any prerelease
    bool this_is_prerelease = !prerelease.empty();
    bool other_is_prerelease = !other.prerelease.empty();

    if (!this_is_prerelease && other_is_prerelease) return false; // release > prerelease
    if (this_is_prerelease && !other_is_prerelease) return true;  // prerelease < release

    // Both are prerelease or both are release
    if (!this_is_prerelease && !other_is_prerelease) return false; // equal

    // Both are prerelease: compare identifiers
    auto this_ids = split(prerelease, '.');
    auto other_ids = split(other.prerelease, '.');

    for (size_t i = 0; i < std::min(this_ids.size(), other_ids.size()); ++i) {
        auto this_id = PrereleaseId::parse(this_ids[i]);
        auto other_id = PrereleaseId::parse(other_ids[i]);
        int cmp = this_id.compare(other_id);
        if (cmp != 0) return cmp < 0;
    }

    // Shorter prerelease has lower precedence
    return this_ids.size() < other_ids.size();
}

bool SemVer::operator<=(const SemVer& other) const noexcept {
    return *this < other || *this == other;
}

bool SemVer::operator>(const SemVer& other) const noexcept {
    return other < *this;
}

bool SemVer::operator>=(const SemVer& other) const noexcept {
    return other <= *this;
}

bool SemVer::operator==(const SemVer& other) const noexcept {
    // Build metadata is ignored in comparison per SemVer spec
    return major == other.major && minor == other.minor && patch == other.patch &&
           prerelease == other.prerelease;
}

bool SemVer::operator!=(const SemVer& other) const noexcept {
    return !(*this == other);
}

bool SemVer::satisfies(std::string_view requirement) const {
    auto req = VersionReq::parse(requirement);
    if (!req.has_value()) {
        return false;
    }
    return req.value().matches(*this);
}

core::Result<VersionReq, core::Error> VersionReq::parse(std::string_view req_str) {
    if (req_str.empty()) {
        return core::fail(core::Error(core::StatusCode::InvalidArgument, "Empty version requirement"));
    }

    VersionReq result;
    std::string_view version_part = req_str;

    // Parse operator
    if (req_str.size() >= 2 && req_str.substr(0, 2) == ">=") {
        result.op = Op::Ge;
        version_part = req_str.substr(2);
    } else if (req_str.size() >= 2 && req_str.substr(0, 2) == "<=") {
        result.op = Op::Le;
        version_part = req_str.substr(2);
    } else if (req_str.size() >= 2 && req_str.substr(0, 2) == "!=") {
        result.op = Op::Ne;
        version_part = req_str.substr(2);
    } else if (req_str.size() >= 1 && req_str[0] == '^') {
        result.op = Op::Caret;
        version_part = req_str.substr(1);
    } else if (req_str.size() >= 1 && req_str[0] == '~') {
        result.op = Op::Tilde;
        version_part = req_str.substr(1);
    } else if (req_str.size() >= 1 && req_str[0] == '>') {
        result.op = Op::Gt;
        version_part = req_str.substr(1);
    } else if (req_str.size() >= 1 && req_str[0] == '<') {
        result.op = Op::Lt;
        version_part = req_str.substr(1);
    } else if (req_str.size() >= 1 && req_str[0] == '=') {
        result.op = Op::Eq;
        version_part = req_str.substr(1);
    } else {
        // Default to exact match
        result.op = Op::Eq;
    }

    auto ver = SemVer::parse(version_part);
    if (!ver.has_value()) {
        return core::fail(ver.error());
    }

    result.version = std::move(ver).value();
    return result;
}

bool VersionReq::matches(const SemVer& v) const noexcept {
    switch (op) {
        case Op::Eq:
            return v == version;
        case Op::Ne:
            return v != version;
        case Op::Lt:
            return v < version;
        case Op::Le:
            return v <= version;
        case Op::Gt:
            return v > version;
        case Op::Ge:
            return v >= version;
        case Op::Caret: {
            // ^1.2.3 = >=1.2.3 <2.0.0
            // ^0.2.3 = >=0.2.3 <0.3.0
            // ^0.0.3 = >=0.0.3 <0.0.4
            if (v < version) return false;
            if (version.major != 0) {
                return v.major == version.major;
            } else if (version.minor != 0) {
                return v.major == 0 && v.minor == version.minor;
            } else {
                return v.major == 0 && v.minor == 0 && v.patch == version.patch;
            }
        }
        case Op::Tilde: {
            // ~1.2.3 = >=1.2.3 <1.3.0
            // ~1.2 = >=1.2.0 <1.3.0
            if (v < version) return false;
            return v.major == version.major && v.minor == version.minor;
        }
    }
    return false;
}

std::string VersionReq::to_string() const {
    std::string op_str;
    switch (op) {
        case Op::Eq:    op_str = "="; break;
        case Op::Ne:    op_str = "!="; break;
        case Op::Lt:    op_str = "<"; break;
        case Op::Le:    op_str = "<="; break;
        case Op::Gt:    op_str = ">"; break;
        case Op::Ge:    op_str = ">="; break;
        case Op::Caret: op_str = "^"; break;
        case Op::Tilde: op_str = "~"; break;
    }
    return op_str + version.to_string();
}

} // namespace pvpgn::scripting::plugin
