// SPDX-License-Identifier: GPL-2.0-or-later
//
// RAII directory iterator.
//
// The legacy code exposed a `pvpgn::Directory` class that wrapped POSIX
// `opendir`/`readdir`/`closedir` (and the Win32 `_findfirst`/`_findnext`
// equivalents) behind a manual open/read/rewind/close interface.  It also
// provided a free function `dir_getfiles()` for recursive extension-filtered
// listing.
//
// This version replaces all of that with `std::filesystem::directory_iterator`
// (C++17/20 standard library — no platform ifdefs needed).
//
// ## Types
//
//   `DirectoryEntry`    — value type: filename stem + full path
//   `DirectoryIterator` — RAII wrapper around `std::filesystem::directory_iterator`
//                         Move-only.  Supports range-for.
//
// ## Free functions (legacy-compatible usage)
//
//   `open_directory(path)`          → `std::optional<DirectoryIterator>`
//   `read_directory(iter)`          → `std::optional<DirectoryEntry>` (advances)
//   `close_directory(iter)`         → `void` (resets to end)
//   `list_files(dir, ext, recurse)` → `std::vector<std::filesystem::path>`
//                                     replaces `dir_getfiles()`
//
// ## Design decisions
//
//   - No exceptions — all error paths return `std::nullopt` or empty containers.
//   - `[[nodiscard]]` on all factory / query functions.
//   - Dot-entries ("." and "..") are automatically skipped by
//     `std::filesystem::directory_iterator`.
//   - `DirectoryIterator` is move-only (mirrors `std::filesystem::directory_iterator`).
//   - The `list_files` extension filter is case-insensitive on all platforms
//     (matches the legacy `strcasecmp` behaviour).

#pragma once

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace pvpgn::v3::infra::compat {

// ---------------------------------------------------------------------------
// DirectoryEntry — value type for a single directory entry
// ---------------------------------------------------------------------------

/// A single entry returned by `DirectoryIterator`.
///
/// Holds both the bare filename (e.g. `"foo.txt"`) and the full path
/// (e.g. `"/var/pvpgn/accounts/foo.txt"`).
struct DirectoryEntry {
    /// Bare filename component (no directory prefix).
    std::filesystem::path name;
    /// Full path as passed to the iterator constructor + the filename.
    std::filesystem::path full_path;

    /// Convenience: is this entry a directory itself?
    [[nodiscard]] bool is_directory() const noexcept {
        std::error_code ec;
        return std::filesystem::is_directory(full_path, ec);
    }

    /// Convenience: is this entry a regular file?
    [[nodiscard]] bool is_regular_file() const noexcept {
        std::error_code ec;
        return std::filesystem::is_regular_file(full_path, ec);
    }
};

// ---------------------------------------------------------------------------
// DirectoryIterator — RAII wrapper around std::filesystem::directory_iterator
// ---------------------------------------------------------------------------

/// RAII directory iterator.
///
/// Wraps `std::filesystem::directory_iterator`.  Move-only (no copy).
/// Supports range-for:
///
/// ```cpp
/// if (auto it = open_directory("/var/pvpgn/accounts")) {
///     for (const auto& entry : *it) {
///         // entry is DirectoryEntry
///     }
/// }
/// ```
///
/// Or use the legacy-compatible free functions:
///
/// ```cpp
/// auto it = open_directory(path);
/// while (auto entry = read_directory(*it)) {
///     process(entry->name.string());
/// }
/// ```
class DirectoryIterator {
public:
    // --- construction -------------------------------------------------------

    /// Construct from a path.  On failure (non-existent / not a directory /
    /// permission denied) the iterator is left in the "end" state and
    /// `is_open()` returns `false`.
    ///
    /// Prefer `open_directory()` which makes the error path explicit via
    /// `std::optional`.
    explicit DirectoryIterator(std::filesystem::path dir) noexcept
        : dir_(std::move(dir))
    {
        std::error_code ec;
        iter_ = std::filesystem::directory_iterator(dir_, ec);
        open_ = !ec;
    }

    // --- move-only ----------------------------------------------------------

    DirectoryIterator(DirectoryIterator&&) noexcept            = default;
    DirectoryIterator& operator=(DirectoryIterator&&) noexcept = default;

    DirectoryIterator(const DirectoryIterator&)            = delete;
    DirectoryIterator& operator=(const DirectoryIterator&) = delete;

    ~DirectoryIterator() = default;

    // --- state queries ------------------------------------------------------

    /// `true` if the directory was opened successfully.
    [[nodiscard]] bool is_open() const noexcept { return open_; }

    /// `true` if there are no more entries to read.
    [[nodiscard]] bool at_end() const noexcept {
        return iter_ == std::filesystem::directory_iterator{};
    }

    // --- range-for support --------------------------------------------------

    /// Sentinel type for range-for end detection.
    struct Sentinel {};

    /// Iterator adaptor that yields `DirectoryEntry` values.
    class Iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type        = DirectoryEntry;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const DirectoryEntry*;
        using reference         = const DirectoryEntry&;

        explicit Iterator(std::filesystem::directory_iterator it) noexcept
            : it_(std::move(it))
        {
            if (it_ != std::filesystem::directory_iterator{}) {
                current_ = make_entry(*it_);
            }
        }

        [[nodiscard]] const DirectoryEntry& operator*() const noexcept {
            return current_;
        }
        [[nodiscard]] const DirectoryEntry* operator->() const noexcept {
            return &current_;
        }

        Iterator& operator++() {
            std::error_code ec;
            it_.increment(ec);
            if (!ec && it_ != std::filesystem::directory_iterator{}) {
                current_ = make_entry(*it_);
            }
            return *this;
        }

        [[nodiscard]] bool operator==(const Sentinel&) const noexcept {
            return it_ == std::filesystem::directory_iterator{};
        }
        [[nodiscard]] bool operator!=(const Sentinel&) const noexcept {
            return it_ != std::filesystem::directory_iterator{};
        }

    private:
        std::filesystem::directory_iterator it_;
        DirectoryEntry                      current_{};

        static DirectoryEntry make_entry(
            const std::filesystem::directory_entry& de) noexcept
        {
            return DirectoryEntry{de.path().filename(), de.path()};
        }
    };

    [[nodiscard]] Iterator begin() const noexcept {
        return Iterator{iter_};
    }
    [[nodiscard]] Sentinel end() const noexcept { return {}; }

    // --- manual advance (legacy-compatible) ---------------------------------

    /// Advance the iterator and return the next entry, or `std::nullopt` when
    /// exhausted.  Skips no entries — `std::filesystem::directory_iterator`
    /// already omits "." and "..".
    [[nodiscard]] std::optional<DirectoryEntry> next() noexcept {
        if (iter_ == std::filesystem::directory_iterator{}) {
            return std::nullopt;
        }
        DirectoryEntry entry{iter_->path().filename(), iter_->path()};
        std::error_code ec;
        iter_.increment(ec);
        return entry;
    }

    /// Reset the iterator back to the beginning of the directory.
    void rewind() noexcept {
        std::error_code ec;
        iter_ = std::filesystem::directory_iterator(dir_, ec);
        open_ = !ec;
    }

    /// Reset the iterator to the end state (equivalent to closing).
    void reset() noexcept {
        iter_ = std::filesystem::directory_iterator{};
        open_ = false;
    }

private:
    std::filesystem::path               dir_;
    std::filesystem::directory_iterator iter_;
    bool                                open_{false};
};

// ---------------------------------------------------------------------------
// Free functions — legacy-compatible API
// ---------------------------------------------------------------------------

/// Open a directory for iteration.
///
/// Returns `std::nullopt` if the path does not exist, is not a directory, or
/// cannot be opened (permission denied, etc.).
///
/// Example:
/// ```cpp
/// auto it = open_directory("/var/pvpgn/accounts");
/// if (!it) { /* handle error */ }
/// ```
[[nodiscard]] inline std::optional<DirectoryIterator>
open_directory(std::filesystem::path dir) noexcept
{
    DirectoryIterator it{std::move(dir)};
    if (!it.is_open()) return std::nullopt;
    return it;
}

/// Advance `iter` and return the next `DirectoryEntry`, or `std::nullopt`
/// when the directory is exhausted.
///
/// Mirrors the legacy `dir.read()` → `const char*` pattern:
/// ```cpp
/// while (auto entry = read_directory(*it)) {
///     process(entry->name.string());
/// }
/// ```
[[nodiscard]] inline std::optional<DirectoryEntry>
read_directory(DirectoryIterator& iter) noexcept
{
    return iter.next();
}

/// Reset `iter` to the end state (equivalent to closing the directory handle).
/// After this call `read_directory(iter)` will return `std::nullopt`.
inline void close_directory(DirectoryIterator& iter) noexcept {
    iter.reset();
}

// ---------------------------------------------------------------------------
// list_files — replaces dir_getfiles()
// ---------------------------------------------------------------------------

namespace detail {

/// Case-insensitive ASCII string comparison helper.
[[nodiscard]] inline bool iequal(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto ca = static_cast<unsigned char>(a[i]);
        const auto cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb)) return false;
    }
    return true;
}

}  // namespace detail

/// Return a list of files under `directory`, optionally filtered by extension
/// and optionally recursive.
///
/// Replaces the legacy `dir_getfiles(directory, ext, recursive)` free function.
///
/// Parameters:
///   `directory` — root directory to scan
///   `ext`       — file extension filter (e.g. `".lua"`, `".xml"`).
///                 Pass `"*"` or an empty string to accept all extensions.
///                 Comparison is case-insensitive (matches legacy behaviour).
///   `recursive` — if `true`, descend into subdirectories
///
/// Returns a `std::vector<std::filesystem::path>` of matching full paths.
/// Subdirectory results are prepended (matching legacy ordering: dirs first,
/// then files).
///
/// Hidden entries (names starting with `.`) are skipped.
[[nodiscard]] inline std::vector<std::filesystem::path>
list_files(const std::filesystem::path& directory,
           std::string_view             ext,
           bool                         recursive) noexcept
{
    std::vector<std::filesystem::path> files;
    std::vector<std::filesystem::path> dir_files;  // from subdirectories

    std::error_code ec;
    std::filesystem::directory_iterator it(directory, ec);
    if (ec) return files;

    const bool any_ext = ext.empty() || ext == "*";

    for (const auto& entry : it) {
        const auto fname = entry.path().filename().string();
        if (fname.empty() || fname[0] == '.') continue;

        std::error_code is_dir_ec;
        if (entry.is_directory(is_dir_ec)) {
            if (recursive) {
                auto sub = list_files(entry.path(), ext, recursive);
                for (auto& p : sub) dir_files.push_back(std::move(p));
            }
            continue;
        }

        if (!any_ext) {
            const auto file_ext = entry.path().extension().string();
            if (!detail::iequal(file_ext, ext)) continue;
        }

        files.push_back(entry.path());
    }

    // Prepend subdirectory results (legacy ordering: dirs first, files after)
    dir_files.insert(dir_files.end(),
                     std::make_move_iterator(files.begin()),
                     std::make_move_iterator(files.end()));
    return dir_files;
}

}  // namespace pvpgn::v3::infra::compat
