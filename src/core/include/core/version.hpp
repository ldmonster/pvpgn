// SPDX-License-Identifier: GPL-2.0-or-later
// Part of PvPGN.
#pragma once

/// @file version.hpp
/// Compile-time version metadata.

namespace pvpgn::core {

inline constexpr int kVersionMajor = 4;
inline constexpr int kVersionMinor = 0;
inline constexpr int kVersionPatch = 0;
inline constexpr const char* kVersionPreRelease = "";

/// Human-readable version string, e.g. "4.0.0".
inline constexpr const char* kVersionString = "4.0.0";

}  // namespace pvpgn::core
