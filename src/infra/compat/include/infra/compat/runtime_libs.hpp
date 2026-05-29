// SPDX-License-Identifier: GPL-2.0-or-later
//
// Cross-platform dynamic library loading.
//
// v3 equivalent of src/compat/runtime_libs.h
//
// The legacy header exposed three macros:
//   OpenLibrary(path)      — dlopen / LoadLibrary
//   GetFunction(handle, f) — dlsym  / GetProcAddress
//   CloseLibrary(handle)   — dlclose / FreeLibrary
//
// The v3 version wraps these in a RAII `DynamicLibrary` class that
// automatically closes the handle on destruction, plus free functions
// that mirror the legacy macro names for migration-aid purposes.
//
// NOTE: The v3 SQL backends should prefer static linking or a proper
// plugin abstraction over raw dlopen. This header is provided as a
// migration aid; new v3 code should use `DynamicLibrary` directly.

#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#if defined(_WIN32) || defined(_WIN64)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
namespace pvpgn::v3::infra::compat::detail {
    using LibHandle = HMODULE;
    inline LibHandle open_lib(const char* path) noexcept {
        return ::LoadLibraryA(path);
    }
    inline void* get_sym(LibHandle h, const char* sym) noexcept {
        return reinterpret_cast<void*>(::GetProcAddress(h, sym));
    }
    inline void close_lib(LibHandle h) noexcept {
        if (h) ::FreeLibrary(h);
    }
}  // namespace pvpgn::v3::infra::compat::detail
#else
#  include <dlfcn.h>
namespace pvpgn::v3::infra::compat::detail {
    using LibHandle = void*;
    inline LibHandle open_lib(const char* path) noexcept {
        return ::dlopen(path, RTLD_LOCAL | RTLD_LAZY);
    }
    inline void* get_sym(LibHandle h, const char* sym) noexcept {
        return ::dlsym(h, sym);
    }
    inline void close_lib(LibHandle h) noexcept {
        if (h) ::dlclose(h);
    }
}  // namespace pvpgn::v3::infra::compat::detail
#endif

namespace pvpgn::v3::infra::compat {

using LibHandle = detail::LibHandle;

/// RAII wrapper for a dynamically loaded shared library.
class DynamicLibrary {
public:
    /// Open a shared library by path.
    /// @throws std::runtime_error if the library cannot be opened.
    explicit DynamicLibrary(std::string_view path)
        : handle_{detail::open_lib(std::string{path}.c_str())}
        , path_{path}
    {
        if (!handle_) {
            throw std::runtime_error{
                std::string{"DynamicLibrary: cannot open '"} + std::string{path} + "'"};
        }
    }

    ~DynamicLibrary() noexcept { detail::close_lib(handle_); }

    // Non-copyable, movable.
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;

    DynamicLibrary(DynamicLibrary&& other) noexcept
        : handle_{other.handle_}, path_{std::move(other.path_)}
    {
        other.handle_ = nullptr;
    }

    DynamicLibrary& operator=(DynamicLibrary&& other) noexcept {
        if (this != &other) {
            detail::close_lib(handle_);
            handle_ = other.handle_;
            path_   = std::move(other.path_);
            other.handle_ = nullptr;
        }
        return *this;
    }

    /// Look up a symbol by name.  Returns nullptr if not found.
    [[nodiscard]] void* symbol(const char* name) const noexcept {
        return detail::get_sym(handle_, name);
    }

    /// @overload
    [[nodiscard]] void* symbol(std::string_view name) const noexcept {
        return symbol(std::string{name}.c_str());
    }

    /// Cast the symbol to a typed function pointer.
    template <typename Fn>
    [[nodiscard]] Fn symbol_as(const char* name) const noexcept {
        return reinterpret_cast<Fn>(symbol(name));
    }

    [[nodiscard]] const std::string& path() const noexcept { return path_; }
    [[nodiscard]] bool is_open() const noexcept { return handle_ != nullptr; }

private:
    LibHandle   handle_{};
    std::string path_;
};

// ---------------------------------------------------------------------------
// Legacy-compatible free functions (migration aid — prefer DynamicLibrary)
// ---------------------------------------------------------------------------

/// Open a shared library.  Returns nullptr on failure.
[[nodiscard]] inline LibHandle open_library(const char* path) noexcept {
    return detail::open_lib(path);
}

/// Look up a symbol in an open library.  Returns nullptr if not found.
[[nodiscard]] inline void* get_function(LibHandle handle, const char* name) noexcept {
    return detail::get_sym(handle, name);
}

/// Close a library handle.
inline void close_library(LibHandle handle) noexcept {
    detail::close_lib(handle);
}

// ---------------------------------------------------------------------------
// Platform library name constants
// ---------------------------------------------------------------------------

#if defined(_WIN32) || defined(_WIN64)
inline constexpr const char* kMysqlLib   = "libmysql.dll";
inline constexpr const char* kPgsqlLib   = "libpq.dll";
inline constexpr const char* kSqlite3Lib = "sqlite3.dll";
inline constexpr const char* kOdbcLib    = "odbc32.dll";
#else
inline constexpr const char* kMysqlLib   = "libmysqlclient.so";
inline constexpr const char* kPgsqlLib   = "libpq.so";
inline constexpr const char* kSqlite3Lib = "libsqlite3.so";
inline constexpr const char* kOdbcLib    = "libodbc.so";
#endif

}  // namespace pvpgn::v3::infra::compat
