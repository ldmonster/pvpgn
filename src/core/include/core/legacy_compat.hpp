// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file legacy_compat.hpp
/// Compatibility shim for migrating legacy xalloc/xstr/scoped_ptr to STL.
/// New v3 code should use STL directly. This header is for migration assistance
/// when gradually converting legacy code to v3 patterns.
///
/// DEPRECATED: All xalloc/xfree/xrealloc/xstrdup wrappers in this header are
/// deprecated. Use std::vector<uint8_t>, std::string, std::make_unique, or
/// std::unique_ptr directly. See docs/history/migration-xalloc-to-stl.md.

#include <memory>
#include <string>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace pvpgn::core::compat {

// ============================================================================
// Drop-in replacements for legacy xalloc functions
// DEPRECATED: Use std::vector<uint8_t> / std::make_unique<T[]> / std::string
// instead. These wrappers exist only to ease incremental migration of legacy
// call sites and will be removed once src/common/xalloc.h is deleted.
// ============================================================================

/// @deprecated Use std::vector<uint8_t>(size) or std::make_unique<T[]>(n) instead.
[[deprecated("use std::vector<uint8_t> or std::make_unique<T[]> instead of xalloc")]]
inline void* xalloc(std::size_t size) {
    return std::malloc(size);
}

/// @deprecated Use std::vector::resize() instead.
[[deprecated("use std::vector::resize() instead of xrealloc")]]
inline void* xrealloc(void* ptr, std::size_t size) {
    return std::realloc(ptr, size);
}

/// @deprecated Use RAII (std::unique_ptr / std::vector / std::string) — no manual free needed.
[[deprecated("use RAII (std::unique_ptr / std::vector / std::string) instead of xfree")]]
inline void xfree(void* ptr) {
    std::free(ptr);
}

// ============================================================================
// Drop-in replacements for legacy xstr functions
// DEPRECATED: Use std::string(s) or pvpgn::core::compat::to_string(s) instead.
// ============================================================================

/// @deprecated Use std::string(s) or pvpgn::core::compat::to_string(s) instead.
[[deprecated("use std::string(s) or pvpgn::core::compat::to_string(s) instead of xstrdup")]]
inline char* xstrdup(const char* s) {
    return s ? strdup(s) : nullptr;
}

// ============================================================================
// Migration helpers: convert legacy char* to std::string safely
// ============================================================================

/// Convert C string to std::string, handling nullptr safely
inline std::string to_string(const char* s) {
    return s ? std::string(s) : std::string{};
}

/// Convert C string with length to std::string, handling nullptr safely
inline std::string to_string(const char* s, std::size_t len) {
    return s ? std::string(s, len) : std::string{};
}

// ============================================================================
// scoped_ptr → unique_ptr aliases for migration
// ============================================================================

/// Alias for std::unique_ptr<T> to replace legacy scoped_ptr<T>
template<typename T>
using scoped_ptr = std::unique_ptr<T>;

/// Alias for std::unique_ptr<T[]> to replace legacy scoped_array<T>
template<typename T>
using scoped_array = std::unique_ptr<T[]>;

// ============================================================================
// Helper functions for common migration patterns
// ============================================================================

/// Create a unique_ptr from a raw pointer (for legacy code that allocates with new)
template<typename T>
inline std::unique_ptr<T> make_scoped(T* ptr) {
    return std::unique_ptr<T>(ptr);
}

/// Create a unique_ptr array from a raw pointer (for legacy code that allocates with new[])
template<typename T>
inline std::unique_ptr<T[]> make_scoped_array(T* ptr) {
    return std::unique_ptr<T[]>(ptr);
}

/// Convert a vector of bytes to a string view (for zero-copy operations)
inline std::string_view bytes_to_view(const std::vector<uint8_t>& bytes) {
    return std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

/// Convert a string to a vector of bytes
inline std::vector<uint8_t> string_to_bytes(const std::string& str) {
    const auto* data = reinterpret_cast<const uint8_t*>(str.data());
    return std::vector<uint8_t>(data, data + str.size());
}

} // namespace pvpgn::core::compat
