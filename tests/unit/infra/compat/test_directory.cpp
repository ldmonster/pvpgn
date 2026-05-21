// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for infra/compat/directory.hpp
//
// Tests cover:
//   - Opening a valid directory succeeds
//   - Opening a non-existent directory returns std::nullopt
//   - Iterating entries via read_directory() free function
//   - Range-for loop over DirectoryIterator
//   - close_directory() stops iteration
//   - rewind() restarts iteration
//   - list_files() with extension filter
//   - list_files() with wildcard ("*") returns all files
//   - list_files() recursive descent
//   - list_files() on non-existent directory returns empty vector
//   - DirectoryEntry::is_directory() / is_regular_file()
//   - Move semantics (DirectoryIterator is move-only)

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "infra/compat/directory.hpp"

namespace compat = pvpgn::v3::infra::compat;
namespace fs     = std::filesystem;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// RAII temporary directory that is removed on destruction.
struct TempDir {
    fs::path path;

    TempDir() {
        path = fs::temp_directory_path()
               / ("pvpgn_dir_test_" + std::to_string(
                      std::hash<std::thread::id>{}(std::this_thread::get_id())));
        fs::create_directories(path);
    }

    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    /// Create a regular file with optional content.
    fs::path make_file(const std::string& name,
                       const std::string& content = "") const
    {
        auto p = path / name;
        std::ofstream f(p);
        f << content;
        return p;
    }

    /// Create a subdirectory.
    fs::path make_subdir(const std::string& name) const {
        auto p = path / name;
        fs::create_directory(p);
        return p;
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// open_directory
// ---------------------------------------------------------------------------

TEST_CASE("open_directory: valid directory returns non-null optional",
          "[infra][compat][directory]")
{
    TempDir tmp;
    auto result = compat::open_directory(tmp.path);
    REQUIRE(result.has_value());
    REQUIRE(result->is_open());
}

TEST_CASE("open_directory: non-existent path returns nullopt",
          "[infra][compat][directory]")
{
    const fs::path bad_path = fs::temp_directory_path() / "pvpgn_no_such_dir_xyz_999";
    auto result = compat::open_directory(bad_path);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("open_directory: regular file path returns nullopt",
          "[infra][compat][directory]")
{
    TempDir tmp;
    auto file = tmp.make_file("not_a_dir.txt");
    auto result = compat::open_directory(file);
    REQUIRE_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// read_directory (free function)
// ---------------------------------------------------------------------------

TEST_CASE("read_directory: empty directory returns nullopt immediately",
          "[infra][compat][directory]")
{
    TempDir tmp;
    auto it = compat::open_directory(tmp.path);
    REQUIRE(it.has_value());

    auto entry = compat::read_directory(*it);
    REQUIRE_FALSE(entry.has_value());
}

TEST_CASE("read_directory: iterates all entries in a flat directory",
          "[infra][compat][directory]")
{
    TempDir tmp;
    tmp.make_file("alpha.txt");
    tmp.make_file("beta.txt");
    tmp.make_file("gamma.txt");

    auto it = compat::open_directory(tmp.path);
    REQUIRE(it.has_value());

    std::set<std::string> names;
    while (auto entry = compat::read_directory(*it)) {
        names.insert(entry->name.string());
    }

    REQUIRE(names.count("alpha.txt") == 1);
    REQUIRE(names.count("beta.txt")  == 1);
    REQUIRE(names.count("gamma.txt") == 1);
    REQUIRE(names.size() == 3);
}

TEST_CASE("read_directory: returns nullopt after exhaustion",
          "[infra][compat][directory]")
{
    TempDir tmp;
    tmp.make_file("only.txt");

    auto it = compat::open_directory(tmp.path);
    REQUIRE(it.has_value());

    // Drain the iterator
    while (compat::read_directory(*it)) {}

    // Further calls must return nullopt
    REQUIRE_FALSE(compat::read_directory(*it).has_value());
    REQUIRE_FALSE(compat::read_directory(*it).has_value());
}

// ---------------------------------------------------------------------------
// close_directory
// ---------------------------------------------------------------------------

TEST_CASE("close_directory: stops iteration",
          "[infra][compat][directory]")
{
    TempDir tmp;
    tmp.make_file("a.txt");
    tmp.make_file("b.txt");

    auto it = compat::open_directory(tmp.path);
    REQUIRE(it.has_value());

    compat::close_directory(*it);

    REQUIRE_FALSE(compat::read_directory(*it).has_value());
}

// ---------------------------------------------------------------------------
// rewind
// ---------------------------------------------------------------------------

TEST_CASE("DirectoryIterator::rewind restarts iteration",
          "[infra][compat][directory]")
{
    TempDir tmp;
    tmp.make_file("x.txt");
    tmp.make_file("y.txt");

    auto it = compat::open_directory(tmp.path);
    REQUIRE(it.has_value());

    // First pass
    std::set<std::string> first_pass;
    while (auto e = compat::read_directory(*it)) {
        first_pass.insert(e->name.string());
    }
    REQUIRE(first_pass.size() == 2);

    // Rewind and second pass
    it->rewind();
    std::set<std::string> second_pass;
    while (auto e = compat::read_directory(*it)) {
        second_pass.insert(e->name.string());
    }

    REQUIRE(first_pass == second_pass);
}

// ---------------------------------------------------------------------------
// Range-for loop
// ---------------------------------------------------------------------------

TEST_CASE("DirectoryIterator: range-for loop yields all entries",
          "[infra][compat][directory]")
{
    TempDir tmp;
    tmp.make_file("one.cfg");
    tmp.make_file("two.cfg");
    tmp.make_subdir("subdir");

    auto it = compat::open_directory(tmp.path);
    REQUIRE(it.has_value());

    std::set<std::string> names;
    for (const auto& entry : *it) {
        names.insert(entry.name.string());
    }

    REQUIRE(names.count("one.cfg")  == 1);
    REQUIRE(names.count("two.cfg")  == 1);
    REQUIRE(names.count("subdir")   == 1);
    REQUIRE(names.size() == 3);
}

TEST_CASE("DirectoryIterator: range-for on empty directory yields nothing",
          "[infra][compat][directory]")
{
    TempDir tmp;
    auto it = compat::open_directory(tmp.path);
    REQUIRE(it.has_value());

    int count = 0;
    for ([[maybe_unused]] const auto& entry : *it) {
        ++count;
    }
    REQUIRE(count == 0);
}

// ---------------------------------------------------------------------------
// DirectoryEntry helpers
// ---------------------------------------------------------------------------

TEST_CASE("DirectoryEntry::is_regular_file and is_directory",
          "[infra][compat][directory]")
{
    TempDir tmp;
    tmp.make_file("file.txt");
    tmp.make_subdir("subdir");

    auto it = compat::open_directory(tmp.path);
    REQUIRE(it.has_value());

    bool found_file = false;
    bool found_dir  = false;

    for (const auto& entry : *it) {
        if (entry.name.string() == "file.txt") {
            REQUIRE(entry.is_regular_file());
            REQUIRE_FALSE(entry.is_directory());
            found_file = true;
        } else if (entry.name.string() == "subdir") {
            REQUIRE(entry.is_directory());
            REQUIRE_FALSE(entry.is_regular_file());
            found_dir = true;
        }
    }

    REQUIRE(found_file);
    REQUIRE(found_dir);
}

TEST_CASE("DirectoryEntry: full_path is absolute and contains name",
          "[infra][compat][directory]")
{
    TempDir tmp;
    tmp.make_file("check.txt");

    auto it = compat::open_directory(tmp.path);
    REQUIRE(it.has_value());

    auto entry = compat::read_directory(*it);
    REQUIRE(entry.has_value());
    REQUIRE(entry->full_path.is_absolute());
    REQUIRE(entry->full_path.filename() == entry->name);
}

// ---------------------------------------------------------------------------
// Move semantics
// ---------------------------------------------------------------------------

TEST_CASE("DirectoryIterator: move construction transfers state",
          "[infra][compat][directory]")
{
    TempDir tmp;
    tmp.make_file("moved.txt");

    auto it = compat::open_directory(tmp.path);
    REQUIRE(it.has_value());

    // Move the iterator out of the optional
    compat::DirectoryIterator moved = std::move(*it);
    REQUIRE(moved.is_open());

    auto entry = compat::read_directory(moved);
    REQUIRE(entry.has_value());
    REQUIRE(entry->name.string() == "moved.txt");
}

TEST_CASE("DirectoryIterator: move assignment transfers state",
          "[infra][compat][directory]")
{
    TempDir tmp;
    tmp.make_file("assign.txt");

    compat::DirectoryIterator a{tmp.path};
    REQUIRE(a.is_open());

    // Construct a second iterator pointing at a non-existent path
    compat::DirectoryIterator b{fs::temp_directory_path() / "pvpgn_no_such_xyz"};
    REQUIRE_FALSE(b.is_open());

    b = std::move(a);
    REQUIRE(b.is_open());

    auto entry = compat::read_directory(b);
    REQUIRE(entry.has_value());
}

// ---------------------------------------------------------------------------
// list_files
// ---------------------------------------------------------------------------

TEST_CASE("list_files: extension filter returns only matching files",
          "[infra][compat][directory]")
{
    TempDir tmp;
    tmp.make_file("script.lua");
    tmp.make_file("config.xml");
    tmp.make_file("readme.txt");

    auto files = compat::list_files(tmp.path, ".lua", false);
    REQUIRE(files.size() == 1);
    REQUIRE(files[0].filename().string() == "script.lua");
}

TEST_CASE("list_files: wildcard '*' returns all files",
          "[infra][compat][directory]")
{
    TempDir tmp;
    tmp.make_file("a.lua");
    tmp.make_file("b.xml");
    tmp.make_file("c.txt");

    auto files = compat::list_files(tmp.path, "*", false);
    REQUIRE(files.size() == 3);
}

TEST_CASE("list_files: empty extension string returns all files",
          "[infra][compat][directory]")
{
    TempDir tmp;
    tmp.make_file("a.lua");
    tmp.make_file("b.xml");

    auto files = compat::list_files(tmp.path, "", false);
    REQUIRE(files.size() == 2);
}

TEST_CASE("list_files: extension filter is case-insensitive",
          "[infra][compat][directory]")
{
    TempDir tmp;
    tmp.make_file("upper.LUA");
    tmp.make_file("lower.lua");
    tmp.make_file("mixed.Lua");

    auto files = compat::list_files(tmp.path, ".lua", false);
    REQUIRE(files.size() == 3);
}

TEST_CASE("list_files: non-matching extension returns empty vector",
          "[infra][compat][directory]")
{
    TempDir tmp;
    tmp.make_file("data.bin");

    auto files = compat::list_files(tmp.path, ".lua", false);
    REQUIRE(files.empty());
}

TEST_CASE("list_files: non-existent directory returns empty vector",
          "[infra][compat][directory]")
{
    const fs::path bad = fs::temp_directory_path() / "pvpgn_no_such_dir_list_xyz";
    auto files = compat::list_files(bad, "*", false);
    REQUIRE(files.empty());
}

TEST_CASE("list_files: recursive=false does not descend into subdirectories",
          "[infra][compat][directory]")
{
    TempDir tmp;
    tmp.make_file("root.lua");
    auto sub = tmp.make_subdir("sub");
    {
        std::ofstream f(sub / "nested.lua");
        f << "-- nested";
    }

    auto files = compat::list_files(tmp.path, ".lua", false);
    REQUIRE(files.size() == 1);
    REQUIRE(files[0].filename().string() == "root.lua");
}

TEST_CASE("list_files: recursive=true descends into subdirectories",
          "[infra][compat][directory]")
{
    TempDir tmp;
    tmp.make_file("root.lua");
    auto sub = tmp.make_subdir("sub");
    {
        std::ofstream f(sub / "nested.lua");
        f << "-- nested";
    }

    auto files = compat::list_files(tmp.path, ".lua", true);
    REQUIRE(files.size() == 2);

    std::set<std::string> names;
    for (const auto& p : files) names.insert(p.filename().string());
    REQUIRE(names.count("root.lua")   == 1);
    REQUIRE(names.count("nested.lua") == 1);
}

TEST_CASE("list_files: hidden entries (dot-prefix) are skipped",
          "[infra][compat][directory]")
{
    TempDir tmp;
    tmp.make_file(".hidden.lua");
    tmp.make_file("visible.lua");

    auto files = compat::list_files(tmp.path, ".lua", false);
    REQUIRE(files.size() == 1);
    REQUIRE(files[0].filename().string() == "visible.lua");
}

TEST_CASE("list_files: subdirectory results are prepended (dirs-first ordering)",
          "[infra][compat][directory]")
{
    TempDir tmp;
    tmp.make_file("z_root.lua");
    auto sub = tmp.make_subdir("sub");
    {
        std::ofstream f(sub / "a_nested.lua");
        f << "-- nested";
    }

    auto files = compat::list_files(tmp.path, ".lua", true);
    REQUIRE(files.size() == 2);
    // Subdirectory file must come first (legacy ordering)
    REQUIRE(files[0].filename().string() == "a_nested.lua");
    REQUIRE(files[1].filename().string() == "z_root.lua");
}
