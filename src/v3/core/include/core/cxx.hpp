// SPDX-License-Identifier: GPL-2.0-or-later
// Part of PvPGN v3.
#pragma once

/// @file cxx.hpp
/// @brief Compile-time C++ language standard guard for the v3 sub-tree.
///
/// R210 (plans/01-modern-cpp-baseline.md): the v3 tree requires C++20.
/// Including this header from any translation unit forces a hard error
/// on older toolchains rather than producing cryptic template diagnostics
/// deep in stdlib code.

#if !defined(__cplusplus)
#error "pvpgn v3 must be compiled as C++ (C++20 or later)."
#endif

// MSVC reports 199711L for __cplusplus unless /Zc:__cplusplus is passed.
// The top-level CMakeLists.txt sets /Zc:__cplusplus for v3 targets via
// pvpgn_v3_target_warnings (see plans/12-build-tooling-ci.md §2).
#if __cplusplus < 202002L
#error "pvpgn v3 requires C++20. Update your compiler / build flags."
#endif

namespace pvpgn::core {

inline constexpr long kCxxStandard = __cplusplus;

}  // namespace pvpgn::core
